#pragma once

#include "bsm_pricer.hpp"
#include "broker_simulator.hpp"
#include "hedging_engine.hpp"
#include "market_data.hpp"
#include "merton_pricer.hpp"
#include "model_calibrator.hpp"

#include <cstddef>
#include <string>
#include <vector>

namespace hedgebot {

struct BacktestConfig {
    double initial_cash;
    double risk_free_rate;
    double dividend_yield;
    double initial_vol_guess;
    bool use_merton;
    std::size_t calibration_window_steps;
    std::size_t option_roll_steps;
    double strike_moneyness;
    OptionType option_type;
    double option_contracts;
    std::string underlying_symbol;

    BacktestConfig()
        : initial_cash(100000.0),
          risk_free_rate(0.02),
          dividend_yield(0.0),
          initial_vol_guess(0.2),
          use_merton(false),
          calibration_window_steps(60),
          option_roll_steps(120),
          strike_moneyness(1.0),
          option_type(OptionType::CALL),
          option_contracts(1.0),
          underlying_symbol("SPOT") {}
};

struct BacktestResult {
    bool success;
    std::string message;
    double total_pnl;
    double sharpe;
    double max_drawdown_pct;
    double hit_rate;
    int num_rehedges;
    std::vector<double> equity_curve;
    std::vector<double> pnl_curve;

    BacktestResult()
        : success(false),
          total_pnl(0.0),
          sharpe(0.0),
          max_drawdown_pct(0.0),
          hit_rate(0.0),
          num_rehedges(0) {}
};

struct InstrumentBacktestResult {
    std::string symbol;
    BacktestResult result;
};

struct BatchBacktestResult {
    bool success;
    std::string message;
    std::vector<InstrumentBacktestResult> instrument_results;
    double average_total_pnl;

    BatchBacktestResult() : success(false), average_total_pnl(0.0) {}
};

class Backtester {
public:
    explicit Backtester(const BacktestConfig& config = BacktestConfig());

    void set_config(const BacktestConfig& config);
    const BacktestConfig& config() const;

    BacktestResult run_from_data(
        const std::vector<PriceBar>& prices,
        const std::vector<OptionQuote>& option_surface,
        const std::vector<RatePoint>& rates,
        const HedgeConfig& hedge_config,
        const ExecutionConfig& execution_config) const;

    BatchBacktestResult run_batch_from_layout(
        const std::string& data_root,
        const HedgeConfig& hedge_config,
        const ExecutionConfig& execution_config,
        const std::string& rates_folder = "RATES") const;

    static double compute_sharpe(const std::vector<double>& pnl_curve);
    static double compute_max_drawdown_pct(const std::vector<double>& equity_curve);
    static double compute_hit_rate(const std::vector<double>& pnl_curve);

private:
    BacktestConfig config_;
};

} // namespace hedgebot
