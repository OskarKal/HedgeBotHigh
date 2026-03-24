#pragma once

#include "common.hpp"

#include <string>
#include <vector>

namespace hedgebot {

struct RiskConfig {
    double max_abs_delta;
    double max_abs_gamma;
    double max_abs_vega;
    double max_leverage;
    double max_drawdown_pct;
    double stop_loss_pct;
    double var_confidence;

    RiskConfig()
        : max_abs_delta(0.20),
          max_abs_gamma(0.05),
          max_abs_vega(1000.0),
          max_leverage(3.0),
          max_drawdown_pct(0.15),
          stop_loss_pct(0.05),
          var_confidence(0.95) {}
};

struct RiskMetrics {
    double var_value;      // Positive loss number at confidence quantile.
    double es_value;       // Expected shortfall (average beyond VaR loss tail).
    double drawdown_pct;   // Current drawdown from peak equity.
    double leverage;

    RiskMetrics() : var_value(0.0), es_value(0.0), drawdown_pct(0.0), leverage(0.0) {}
};

struct RiskDecision {
    bool within_limits;
    bool breach_delta;
    bool breach_gamma;
    bool breach_vega;
    bool breach_leverage;
    bool breach_drawdown;
    bool stop_loss_triggered;
    bool circuit_breaker_triggered;
    std::string reason;

    RiskDecision()
        : within_limits(true),
          breach_delta(false),
          breach_gamma(false),
          breach_vega(false),
          breach_leverage(false),
          breach_drawdown(false),
          stop_loss_triggered(false),
          circuit_breaker_triggered(false) {}
};

class RiskManager {
public:
    explicit RiskManager(const RiskConfig& config = RiskConfig());

    void set_config(const RiskConfig& config);
    const RiskConfig& config() const;

    static std::vector<double> returns_from_prices(const std::vector<double>& prices);

    static RiskMetrics historical_var_es(const std::vector<double>& pnl_samples, double confidence);

    static double compute_leverage(double gross_notional, double equity);

    static double compute_drawdown_pct(double current_equity, double peak_equity);

    RiskDecision evaluate(
        const Greeks& portfolio_greeks,
        double gross_notional,
        double equity,
        double peak_equity,
        const std::vector<double>& pnl_samples,
        double intraday_pnl,
        double day_start_equity) const;

private:
    RiskConfig config_;
};

} // namespace hedgebot
