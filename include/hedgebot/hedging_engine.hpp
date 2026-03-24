#pragma once

#include "common.hpp"

#include <cstdint>
#include <string>

namespace hedgebot {

struct HedgeConfig {
    double delta_threshold;
    double gamma_threshold;
    double vega_threshold;
    std::int64_t rebalance_interval_seconds;
    double target_delta;
    double target_gamma;
    double target_vega;
    double max_underlying_trade;

    HedgeConfig()
        : delta_threshold(0.05),
          gamma_threshold(0.01),
          vega_threshold(0.10),
          rebalance_interval_seconds(3600),
          target_delta(0.0),
          target_gamma(0.0),
          target_vega(0.0),
          max_underlying_trade(1e9) {}
};

struct PortfolioGreeksSnapshot {
    double underlying_units;
    Greeks option_greeks_per_contract;
    double option_contracts;
    std::int64_t last_rebalance_timestamp;

    PortfolioGreeksSnapshot()
        : underlying_units(0.0), option_contracts(0.0), last_rebalance_timestamp(0) {}
};

struct RehedgeDecision {
    bool should_rehedge;
    bool due_to_time;
    bool due_to_threshold;
    Greeks current_total_greeks;
    std::string reason;

    RehedgeDecision() : should_rehedge(false), due_to_time(false), due_to_threshold(false) {}
};

struct HedgeInstrumentGreeks {
    bool enabled;
    double delta_per_unit;
    double gamma_per_unit;
    double vega_per_unit;

    HedgeInstrumentGreeks()
        : enabled(false), delta_per_unit(0.0), gamma_per_unit(0.0), vega_per_unit(0.0) {}
};

struct HedgePlan {
    double underlying_trade_units;
    double gamma_hedge_units;
    Greeks pre_trade_greeks;
    Greeks projected_post_trade_greeks;

    HedgePlan() : underlying_trade_units(0.0), gamma_hedge_units(0.0) {}
};

class HedgingEngine {
public:
    explicit HedgingEngine(const HedgeConfig& config = HedgeConfig());

    void set_config(const HedgeConfig& config);
    const HedgeConfig& config() const;

    Greeks compute_portfolio_greeks(const PortfolioGreeksSnapshot& snapshot) const;

    RehedgeDecision evaluate_rehedge(
        const PortfolioGreeksSnapshot& snapshot,
        std::int64_t current_timestamp) const;

    HedgePlan build_delta_gamma_hedge_plan(
        const PortfolioGreeksSnapshot& snapshot,
        const HedgeInstrumentGreeks& gamma_hedge_instrument = HedgeInstrumentGreeks()) const;

    // N = beta * notional_exposure / (futures_price * contract_multiplier)
    static double compute_beta_weighted_futures_contracts(
        double beta,
        double notional_exposure,
        double futures_price,
        double contract_multiplier);

private:
    HedgeConfig config_;

    static double clamp_abs(double value, double abs_limit);
};

} // namespace hedgebot
