#include "hedgebot/risk_manager.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <stdexcept>

namespace hedgebot {

RiskManager::RiskManager(const RiskConfig& config) : config_(config) {}

void RiskManager::set_config(const RiskConfig& config) {
    config_ = config;
}

const RiskConfig& RiskManager::config() const {
    return config_;
}

std::vector<double> RiskManager::returns_from_prices(const std::vector<double>& prices) {
    if (prices.size() < 2) {
        return {};
    }

    std::vector<double> ret;
    ret.reserve(prices.size() - 1);
    for (std::size_t i = 1; i < prices.size(); ++i) {
        if (prices[i - 1] <= 0.0 || prices[i] <= 0.0) {
            throw std::invalid_argument("returns_from_prices requires strictly positive prices");
        }
        ret.push_back(prices[i] / prices[i - 1] - 1.0);
    }
    return ret;
}

RiskMetrics RiskManager::historical_var_es(const std::vector<double>& pnl_samples, double confidence) {
    if (pnl_samples.empty()) {
        throw std::invalid_argument("historical_var_es requires non-empty pnl samples");
    }
    if (confidence <= 0.0 || confidence >= 1.0) {
        throw std::invalid_argument("historical_var_es requires confidence in (0,1)");
    }

    // Convert to losses so right tail is bad outcomes.
    std::vector<double> losses;
    losses.reserve(pnl_samples.size());
    for (double pnl : pnl_samples) {
        losses.push_back(-pnl);
    }
    std::sort(losses.begin(), losses.end());

    const std::size_t n = losses.size();
    const std::size_t idx = static_cast<std::size_t>(std::floor(confidence * static_cast<double>(n - 1)));

    RiskMetrics out;
    out.var_value = losses[idx];

    const std::size_t tail_start = idx;
    double tail_sum = 0.0;
    for (std::size_t i = tail_start; i < n; ++i) {
        tail_sum += losses[i];
    }
    out.es_value = tail_sum / static_cast<double>(n - tail_start);
    return out;
}

double RiskManager::compute_leverage(double gross_notional, double equity) {
    if (equity <= 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    return std::abs(gross_notional) / equity;
}

double RiskManager::compute_drawdown_pct(double current_equity, double peak_equity) {
    if (peak_equity <= 0.0) {
        throw std::invalid_argument("compute_drawdown_pct requires positive peak_equity");
    }
    if (current_equity >= peak_equity) {
        return 0.0;
    }
    return (peak_equity - current_equity) / peak_equity;
}

RiskDecision RiskManager::evaluate(
    const Greeks& portfolio_greeks,
    double gross_notional,
    double equity,
    double peak_equity,
    const std::vector<double>& pnl_samples,
    double intraday_pnl,
    double day_start_equity) const {
    RiskDecision decision;

    RiskMetrics metrics;
    if (!pnl_samples.empty()) {
        metrics = historical_var_es(pnl_samples, config_.var_confidence);
    }
    metrics.leverage = compute_leverage(gross_notional, equity);
    metrics.drawdown_pct = compute_drawdown_pct(equity, peak_equity);

    decision.breach_delta = std::abs(portfolio_greeks.delta) > config_.max_abs_delta;
    decision.breach_gamma = std::abs(portfolio_greeks.gamma) > config_.max_abs_gamma;
    decision.breach_vega = std::abs(portfolio_greeks.vega) > config_.max_abs_vega;
    decision.breach_leverage = metrics.leverage > config_.max_leverage;
    decision.breach_drawdown = metrics.drawdown_pct > config_.max_drawdown_pct;

    if (day_start_equity > 0.0) {
        const double stop_loss_level = -config_.stop_loss_pct * day_start_equity;
        decision.stop_loss_triggered = intraday_pnl <= stop_loss_level;
    }
    decision.circuit_breaker_triggered = decision.breach_drawdown;

    decision.within_limits = !(decision.breach_delta ||
                               decision.breach_gamma ||
                               decision.breach_vega ||
                               decision.breach_leverage ||
                               decision.breach_drawdown ||
                               decision.stop_loss_triggered ||
                               decision.circuit_breaker_triggered);

    if (decision.within_limits) {
        decision.reason = "ok";
    } else if (decision.stop_loss_triggered) {
        decision.reason = "stop_loss";
    } else if (decision.breach_drawdown) {
        decision.reason = "drawdown";
    } else if (decision.breach_leverage) {
        decision.reason = "leverage";
    } else if (decision.breach_delta) {
        decision.reason = "delta";
    } else if (decision.breach_gamma) {
        decision.reason = "gamma";
    } else if (decision.breach_vega) {
        decision.reason = "vega";
    } else {
        decision.reason = "unknown";
    }

    return decision;
}

} // namespace hedgebot
