#include "hedgebot/backtester.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>

namespace hedgebot {

namespace {

double latest_rate_before(const std::vector<RatePoint>& rates, std::int64_t ts, double fallback) {
    double r = fallback;
    for (const auto& rp : rates) {
        if (rp.timestamp <= ts) {
            r = rp.rate;
        } else {
            break;
        }
    }
    return r;
}

std::vector<PriceBar> sorted_prices(const std::vector<PriceBar>& input) {
    auto p = input;
    std::sort(p.begin(), p.end(), [](const PriceBar& a, const PriceBar& b) {
        return a.timestamp < b.timestamp;
    });
    return p;
}

} // namespace

Backtester::Backtester(const BacktestConfig& config) : config_(config) {}

void Backtester::set_config(const BacktestConfig& config) {
    config_ = config;
}

const BacktestConfig& Backtester::config() const {
    return config_;
}

double Backtester::compute_sharpe(const std::vector<double>& pnl_curve) {
    if (pnl_curve.size() < 2) {
        return 0.0;
    }
    const double mean = std::accumulate(pnl_curve.begin(), pnl_curve.end(), 0.0) / static_cast<double>(pnl_curve.size());
    double var = 0.0;
    for (double x : pnl_curve) {
        const double d = x - mean;
        var += d * d;
    }
    var /= static_cast<double>(pnl_curve.size());
    const double sd = std::sqrt(var);
    if (sd < 1e-12) {
        return 0.0;
    }
    return mean / sd * std::sqrt(static_cast<double>(pnl_curve.size()));
}

double Backtester::compute_max_drawdown_pct(const std::vector<double>& equity_curve) {
    if (equity_curve.empty()) {
        return 0.0;
    }
    double peak = equity_curve.front();
    double max_dd = 0.0;
    for (double e : equity_curve) {
        peak = std::max(peak, e);
        if (peak > 0.0) {
            max_dd = std::max(max_dd, (peak - e) / peak);
        }
    }
    return max_dd;
}

double Backtester::compute_hit_rate(const std::vector<double>& pnl_curve) {
    if (pnl_curve.empty()) {
        return 0.0;
    }
    std::size_t wins = 0;
    for (double p : pnl_curve) {
        if (p > 0.0) {
            ++wins;
        }
    }
    return static_cast<double>(wins) / static_cast<double>(pnl_curve.size());
}

BacktestResult Backtester::run_from_data(
    const std::vector<PriceBar>& prices,
    const std::vector<OptionQuote>& option_surface,
    const std::vector<RatePoint>& rates,
    const HedgeConfig& hedge_config,
    const ExecutionConfig& execution_config) const {
    BacktestResult result;
    const auto px = sorted_prices(prices);
    if (px.size() < 3) {
        result.message = "Need at least 3 price bars";
        return result;
    }

    HedgingEngine hedger(hedge_config);
    BrokerSimulator broker(execution_config, config_.initial_cash);
    ModelCalibrator calibrator;
    BSM_Pricer bsm;

    double calibrated_sigma = config_.initial_vol_guess;
    if (!option_surface.empty()) {
        MarketData calib_md;
        calib_md.spot_price = px.front().close;
        calib_md.risk_free_rate = config_.risk_free_rate;
        calib_md.dividend_yield = config_.dividend_yield;
        calib_md.time_to_expiry = option_surface.front().maturity;
        calib_md.current_vol = config_.initial_vol_guess;
        const auto bsm_fit = calibrator.calibrate_bsm_vol(option_surface, calib_md, 0.01, 2.0, 80);
        calibrated_sigma = bsm_fit.sigma;
    }

    const double initial_strike = px.front().close * config_.strike_moneyness;
    OptionData current_option(OptionType::CALL, initial_strike, 30.0 / 365.0, "BT_CALL");
    double option_qty = 1.0;

    MarketData md0;
    md0.spot_price = px.front().close;
    md0.risk_free_rate = config_.risk_free_rate;
    md0.dividend_yield = config_.dividend_yield;
    md0.time_to_expiry = current_option.expiry();
    md0.current_vol = calibrated_sigma;
    const double option_entry = bsm.price(current_option, md0);
    double option_cash = -option_qty * option_entry;

    double prev_total_equity = config_.initial_cash + option_cash + option_qty * option_entry;
    std::int64_t last_rehedge_ts = px.front().timestamp;

    for (std::size_t i = 1; i < px.size(); ++i) {
        const std::int64_t ts_prev = px[i - 1].timestamp;
        const std::int64_t ts_cur = px[i].timestamp;
        const double step_years = std::max(1.0 / 31536000.0, static_cast<double>(ts_cur - ts_prev) / 31536000.0);

        double remaining_t = std::max(1e-6, current_option.expiry() - static_cast<double>(i) * step_years);
        if (config_.option_roll_steps > 0 && (i % config_.option_roll_steps == 0)) {
            const double roll_strike = px[i - 1].close * config_.strike_moneyness;
            current_option = OptionData(OptionType::CALL, roll_strike, 30.0 / 365.0, "BT_CALL");
            remaining_t = current_option.expiry();
        }

        // Recalibrate periodically from the provided surface snapshot.
        if (!option_surface.empty() && config_.calibration_window_steps > 0 && (i % config_.calibration_window_steps == 0)) {
            MarketData calib_md;
            calib_md.spot_price = px[i - 1].close;
            calib_md.risk_free_rate = latest_rate_before(rates, ts_prev, config_.risk_free_rate);
            calib_md.dividend_yield = config_.dividend_yield;
            calib_md.time_to_expiry = option_surface.front().maturity;
            calib_md.current_vol = calibrated_sigma;
            const auto bsm_fit = calibrator.calibrate_bsm_vol(option_surface, calib_md, 0.01, 2.0, 80);
            calibrated_sigma = bsm_fit.sigma;
        }

        MarketData md_prev;
        md_prev.spot_price = px[i - 1].close;
        md_prev.risk_free_rate = latest_rate_before(rates, ts_prev, config_.risk_free_rate);
        md_prev.dividend_yield = config_.dividend_yield;
        md_prev.time_to_expiry = remaining_t;
        md_prev.current_vol = calibrated_sigma;

        // Build hedging snapshot from current option greek and current broker underlying exposure.
        const Greeks option_g = bsm.calculate_greeks(current_option, md_prev);
        const PositionState pos = broker.get_position(config_.underlying_symbol);

        PortfolioGreeksSnapshot snap;
        snap.underlying_units = pos.quantity;
        snap.option_greeks_per_contract = option_g;
        snap.option_contracts = option_qty;
        snap.last_rebalance_timestamp = last_rehedge_ts;

        const auto decision = hedger.evaluate_rehedge(snap, ts_prev);
        if (decision.should_rehedge) {
            const auto plan = hedger.build_delta_gamma_hedge_plan(snap);
            if (std::abs(plan.underlying_trade_units) > 1e-12) {
                broker.send_order({config_.underlying_symbol, plan.underlying_trade_units, px[i - 1].close, ts_prev});
                last_rehedge_ts = ts_prev;
                result.num_rehedges += 1;
            }
        }

        broker.mark_price(config_.underlying_symbol, px[i].close);

        MarketData md_cur = md_prev;
        md_cur.spot_price = px[i].close;
        md_cur.risk_free_rate = latest_rate_before(rates, ts_cur, config_.risk_free_rate);
        md_cur.time_to_expiry = std::max(1e-6, remaining_t - step_years);

        const double option_mtM = option_qty * bsm.price(current_option, md_cur);
        const double total_equity = broker.get_total_equity() + option_cash + option_mtM;

        result.equity_curve.push_back(total_equity);
        result.pnl_curve.push_back(total_equity - prev_total_equity);
        prev_total_equity = total_equity;
    }

    if (result.equity_curve.empty()) {
        result.message = "No equity points generated";
        return result;
    }

    result.total_pnl = result.equity_curve.back() - config_.initial_cash;
    result.sharpe = compute_sharpe(result.pnl_curve);
    result.max_drawdown_pct = compute_max_drawdown_pct(result.equity_curve);
    result.hit_rate = compute_hit_rate(result.pnl_curve);
    result.success = true;
    result.message = "ok";
    return result;
}

} // namespace hedgebot
