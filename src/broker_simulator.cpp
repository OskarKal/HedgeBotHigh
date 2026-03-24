#include "hedgebot/broker_simulator.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hedgebot {

BrokerSimulator::BrokerSimulator(const ExecutionConfig& config, double initial_cash)
    : config_(config), cash_(initial_cash) {}

int BrokerSimulator::sign(double x) {
    if (x > 0.0) {
        return 1;
    }
    if (x < 0.0) {
        return -1;
    }
    return 0;
}

double BrokerSimulator::abs_min(double a, double b) {
    return std::min(std::abs(a), std::abs(b));
}

FillReport BrokerSimulator::send_order(const OrderRequest& order) {
    FillReport report;
    report.symbol = order.symbol;
    report.filled_quantity = order.quantity;
    report.requested_price = order.limit_price;

    if (order.symbol.empty()) {
        report.message = "Order rejected: empty symbol";
        return report;
    }
    if (std::abs(order.quantity) < 1e-12) {
        report.message = "Order rejected: zero quantity";
        return report;
    }
    if (order.limit_price <= 0.0) {
        report.message = "Order rejected: non-positive price";
        return report;
    }

    const int side = sign(order.quantity);
    const double slippage = config_.slippage_bps / 10000.0;
    report.fill_price = order.limit_price * (1.0 + static_cast<double>(side) * slippage);
    report.notional = std::abs(order.quantity) * report.fill_price;
    report.commission = config_.fixed_commission + config_.commission_rate * report.notional;
    report.cash_impact = -order.quantity * report.fill_price - report.commission;

    PositionState& pos = positions_[order.symbol];

    // Realized PnL from closing leg.
    if (std::abs(pos.quantity) > 1e-12 && sign(pos.quantity) != sign(order.quantity)) {
        const double close_qty = abs_min(pos.quantity, order.quantity);
        const double realized = close_qty * (report.fill_price - pos.average_price) * static_cast<double>(sign(pos.quantity));
        pos.realized_pnl += realized;
    }

    const double old_qty = pos.quantity;
    const double new_qty = old_qty + order.quantity;

    if (std::abs(old_qty) < 1e-12 || sign(old_qty) == sign(order.quantity)) {
        // Increasing same-side position: weighted average entry price.
        const double old_notional = std::abs(old_qty) * pos.average_price;
        const double add_notional = std::abs(order.quantity) * report.fill_price;
        pos.quantity = new_qty;
        pos.average_price = (old_notional + add_notional) / std::abs(new_qty);
    } else {
        // Reducing, flattening, or flipping.
        pos.quantity = new_qty;
        if (std::abs(new_qty) < 1e-12) {
            pos.quantity = 0.0;
            pos.average_price = 0.0;
        } else if (sign(new_qty) != sign(old_qty)) {
            // Flipped side: remaining quantity starts at the current fill price.
            pos.average_price = report.fill_price;
        }
    }

    cash_ += report.cash_impact;
    marks_[order.symbol] = report.fill_price;

    report.success = true;
    report.message = "filled";
    return report;
}

void BrokerSimulator::mark_price(const std::string& symbol, double market_price) {
    if (symbol.empty()) {
        throw std::invalid_argument("mark_price: symbol cannot be empty");
    }
    if (market_price <= 0.0) {
        throw std::invalid_argument("mark_price: market_price must be positive");
    }
    marks_[symbol] = market_price;
}

PositionState BrokerSimulator::get_position(const std::string& symbol) const {
    const auto it = positions_.find(symbol);
    if (it == positions_.end()) {
        return PositionState();
    }
    return it->second;
}

double BrokerSimulator::get_unrealized_pnl(const std::string& symbol) const {
    const auto pit = positions_.find(symbol);
    if (pit == positions_.end()) {
        return 0.0;
    }
    const PositionState& pos = pit->second;
    if (std::abs(pos.quantity) < 1e-12) {
        return 0.0;
    }

    const auto mit = marks_.find(symbol);
    const double mark = (mit == marks_.end()) ? pos.average_price : mit->second;
    return pos.quantity * (mark - pos.average_price);
}

double BrokerSimulator::get_total_unrealized_pnl() const {
    double total = 0.0;
    for (const auto& kv : positions_) {
        total += get_unrealized_pnl(kv.first);
    }
    return total;
}

double BrokerSimulator::get_total_realized_pnl() const {
    double total = 0.0;
    for (const auto& kv : positions_) {
        total += kv.second.realized_pnl;
    }
    return total;
}

double BrokerSimulator::get_cash() const {
    return cash_;
}

double BrokerSimulator::get_total_equity() const {
    return cash_ + get_total_unrealized_pnl();
}

} // namespace hedgebot
