#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

namespace hedgebot {

struct ExecutionConfig {
    double fixed_commission;
    double commission_rate;
    double slippage_bps;

    ExecutionConfig() : fixed_commission(0.0), commission_rate(0.0), slippage_bps(0.0) {}
};

struct OrderRequest {
    std::string symbol;
    double quantity;      // Positive buy, negative sell.
    double limit_price;   // For simulator we treat this as reference execution price.
    std::int64_t timestamp;
};

struct FillReport {
    bool success;
    std::string symbol;
    double filled_quantity;
    double requested_price;
    double fill_price;
    double notional;
    double commission;
    double cash_impact;
    std::string message;

    FillReport()
        : success(false),
          filled_quantity(0.0),
          requested_price(0.0),
          fill_price(0.0),
          notional(0.0),
          commission(0.0),
          cash_impact(0.0) {}
};

struct PositionState {
    double quantity;
    double average_price;
    double realized_pnl;

    PositionState() : quantity(0.0), average_price(0.0), realized_pnl(0.0) {}
};

class BrokerSimulator {
public:
    explicit BrokerSimulator(
        const ExecutionConfig& config = ExecutionConfig(),
        double initial_cash = 0.0);

    FillReport send_order(const OrderRequest& order);

    void mark_price(const std::string& symbol, double market_price);
    PositionState get_position(const std::string& symbol) const;

    double get_unrealized_pnl(const std::string& symbol) const;
    double get_total_unrealized_pnl() const;
    double get_total_realized_pnl() const;
    double get_cash() const;
    double get_total_equity() const;

private:
    ExecutionConfig config_;
    double cash_;
    std::unordered_map<std::string, PositionState> positions_;
    std::unordered_map<std::string, double> marks_;

    static int sign(double x);
    static double abs_min(double a, double b);
};

} // namespace hedgebot
