#include "hedgebot/hedging_engine.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace hedgebot {

HedgingEngine::HedgingEngine(const HedgeConfig& config) : config_(config) {}

void HedgingEngine::set_config(const HedgeConfig& config) {
    config_ = config;
}

const HedgeConfig& HedgingEngine::config() const {
    return config_;
}

double HedgingEngine::clamp_abs(double value, double abs_limit) {
    if (abs_limit <= 0.0) {
        return 0.0;
    }
    if (value > abs_limit) {
        return abs_limit;
    }
    if (value < -abs_limit) {
        return -abs_limit;
    }
    return value;
}

Greeks HedgingEngine::compute_portfolio_greeks(const PortfolioGreeksSnapshot& snapshot) const {
    Greeks out;
    const double n = snapshot.option_contracts;

    out.delta = snapshot.underlying_units + n * snapshot.option_greeks_per_contract.delta;
    out.gamma = n * snapshot.option_greeks_per_contract.gamma;
    out.vega = n * snapshot.option_greeks_per_contract.vega;
    out.theta = n * snapshot.option_greeks_per_contract.theta;
    out.rho = n * snapshot.option_greeks_per_contract.rho;
    out.vanna = n * snapshot.option_greeks_per_contract.vanna;
    out.volga = n * snapshot.option_greeks_per_contract.volga;

    return out;
}

RehedgeDecision HedgingEngine::evaluate_rehedge(
    const PortfolioGreeksSnapshot& snapshot,
    std::int64_t current_timestamp) const {
    RehedgeDecision decision;
    decision.current_total_greeks = compute_portfolio_greeks(snapshot);

    const bool delta_breach = std::abs(decision.current_total_greeks.delta - config_.target_delta) > config_.delta_threshold;
    const bool gamma_breach = std::abs(decision.current_total_greeks.gamma - config_.target_gamma) > config_.gamma_threshold;
    const bool vega_breach = std::abs(decision.current_total_greeks.vega - config_.target_vega) > config_.vega_threshold;

    decision.due_to_threshold = delta_breach || gamma_breach || vega_breach;

    if (snapshot.last_rebalance_timestamp > 0 && config_.rebalance_interval_seconds > 0) {
        const std::int64_t elapsed = current_timestamp - snapshot.last_rebalance_timestamp;
        decision.due_to_time = elapsed >= config_.rebalance_interval_seconds;
    } else {
        decision.due_to_time = false;
    }

    decision.should_rehedge = decision.due_to_threshold || decision.due_to_time;

    if (decision.should_rehedge) {
        if (decision.due_to_threshold && decision.due_to_time) {
            decision.reason = "threshold+time";
        } else if (decision.due_to_threshold) {
            decision.reason = "threshold";
        } else {
            decision.reason = "time";
        }
    } else {
        decision.reason = "none";
    }

    return decision;
}

HedgePlan HedgingEngine::build_delta_gamma_hedge_plan(
    const PortfolioGreeksSnapshot& snapshot,
    const HedgeInstrumentGreeks& gamma_hedge_instrument) const {
    HedgePlan plan;
    plan.pre_trade_greeks = compute_portfolio_greeks(snapshot);

    const double delta_gap = plan.pre_trade_greeks.delta - config_.target_delta;
    const double gamma_gap = plan.pre_trade_greeks.gamma - config_.target_gamma;

    // Step 1: use an optional hedge instrument (usually an option) to offset gamma.
    if (gamma_hedge_instrument.enabled && std::abs(gamma_hedge_instrument.gamma_per_unit) > 1e-12) {
        plan.gamma_hedge_units = -gamma_gap / gamma_hedge_instrument.gamma_per_unit;
    }

    // Step 2: use the underlying to clean residual delta after gamma hedge trade impact.
    const double residual_delta = delta_gap + plan.gamma_hedge_units * gamma_hedge_instrument.delta_per_unit;
    plan.underlying_trade_units = -residual_delta;
    plan.underlying_trade_units = clamp_abs(plan.underlying_trade_units, config_.max_underlying_trade);

    plan.projected_post_trade_greeks = plan.pre_trade_greeks;
    plan.projected_post_trade_greeks.delta += plan.underlying_trade_units;
    plan.projected_post_trade_greeks.delta += plan.gamma_hedge_units * gamma_hedge_instrument.delta_per_unit;
    plan.projected_post_trade_greeks.gamma += plan.gamma_hedge_units * gamma_hedge_instrument.gamma_per_unit;
    plan.projected_post_trade_greeks.vega += plan.gamma_hedge_units * gamma_hedge_instrument.vega_per_unit;

    return plan;
}

double HedgingEngine::compute_beta_weighted_futures_contracts(
    double beta,
    double notional_exposure,
    double futures_price,
    double contract_multiplier) {
    if (futures_price <= 0.0 || contract_multiplier <= 0.0) {
        throw std::invalid_argument("futures_price and contract_multiplier must be positive");
    }

    return beta * notional_exposure / (futures_price * contract_multiplier);
}

} // namespace hedgebot
