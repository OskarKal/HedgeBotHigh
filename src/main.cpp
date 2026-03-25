#include "hedgebot/backtester.hpp"
#include "hedgebot/bsm_pricer.hpp"
#include "hedgebot/hedging_engine.hpp"
#include "hedgebot/market_data.hpp"
#include "hedgebot/merton_pricer.hpp"
#include "hedgebot/option_data.hpp"
#include "hedgebot/common.hpp"

#include <filesystem>
#include <iostream>
#include <iomanip>
#include <cmath>
#include <limits>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <sstream>

using namespace hedgebot;

struct HedgeScenario {
    std::string name;
    BacktestConfig backtest;
    HedgeConfig hedge;
};

struct ScenarioBatchResult {
    std::string name;
    BacktestConfig backtest;
    HedgeConfig hedge;
    BatchBacktestResult batch;
    double avg_sharpe;
};

double compute_avg_sharpe(const BatchBacktestResult& batch) {
    if (!batch.success || batch.instrument_results.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    int n = 0;
    for (const auto& one : batch.instrument_results) {
        if (one.result.success) {
            sum += one.result.sharpe;
            ++n;
        }
    }
    if (n == 0) {
        return 0.0;
    }
    return sum / static_cast<double>(n);
}

const char* option_type_name(OptionType type) {
    return type == OptionType::CALL ? "CALL" : "PUT";
}

bool heavy_mode_enabled() {
    const char* v = std::getenv("HEDGEBOT_HEAVY_SWEEP");
    if (v == nullptr) {
        return false;
    }
    const std::string s(v);
    return s == "1" || s == "true" || s == "TRUE" || s == "yes" || s == "YES";
}

std::string to_compact(double x, int precision = 3) {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(precision) << x;
    return oss.str();
}

void save_sweep_csv(
    const std::filesystem::path& out_path,
    const std::vector<ScenarioBatchResult>& sweep_results) {
    std::ofstream out(out_path);
    if (!out) {
        return;
    }

    out << "scenario,success,avg_pnl,avg_sharpe,instrument_count,"
           "option_type,option_contracts,strike_moneyness,delta_threshold,"
           "gamma_threshold,vega_threshold,rebalance_interval_seconds\n";

    for (const auto& one : sweep_results) {
        out << one.name << ","
            << (one.batch.success ? "1" : "0") << ","
            << one.batch.average_total_pnl << ","
            << one.avg_sharpe << ","
            << one.batch.instrument_results.size() << ","
            << option_type_name(one.backtest.option_type) << ","
            << one.backtest.option_contracts << ","
            << one.backtest.strike_moneyness << ","
            << one.hedge.delta_threshold << ","
            << one.hedge.gamma_threshold << ","
            << one.hedge.vega_threshold << ","
            << one.hedge.rebalance_interval_seconds << "\n";
    }
}

void save_best_config_yaml(
    const std::filesystem::path& out_path,
    const ScenarioBatchResult& best,
    const std::string& data_root) {
    std::ofstream out(out_path);
    if (!out) {
        return;
    }

    out << "best_strategy:\n";
    out << "  scenario_name: \"" << best.name << "\"\n";
    out << "  data_root: \"" << data_root << "\"\n";
    out << "  objective:\n";
    out << "    avg_pnl: " << best.batch.average_total_pnl << "\n";
    out << "    avg_sharpe: " << best.avg_sharpe << "\n";
    out << "  backtest:\n";
    out << "    initial_cash: " << best.backtest.initial_cash << "\n";
    out << "    risk_free_rate: " << best.backtest.risk_free_rate << "\n";
    out << "    dividend_yield: " << best.backtest.dividend_yield << "\n";
    out << "    calibration_window_steps: " << best.backtest.calibration_window_steps << "\n";
    out << "    option_roll_steps: " << best.backtest.option_roll_steps << "\n";
    out << "    strike_moneyness: " << best.backtest.strike_moneyness << "\n";
    out << "    option_type: \"" << option_type_name(best.backtest.option_type) << "\"\n";
    out << "    option_contracts: " << best.backtest.option_contracts << "\n";
    out << "  hedge:\n";
    out << "    delta_threshold: " << best.hedge.delta_threshold << "\n";
    out << "    gamma_threshold: " << best.hedge.gamma_threshold << "\n";
    out << "    vega_threshold: " << best.hedge.vega_threshold << "\n";
    out << "    rebalance_interval_seconds: " << best.hedge.rebalance_interval_seconds << "\n";
}

void print_greeks(const Greeks& greeks) {
    std::cout << std::fixed << std::setprecision(6);
    std::cout << "  Delta:   " << greeks.delta << "\n";
    std::cout << "  Gamma:   " << greeks.gamma << "\n";
    std::cout << "  Vega:    " << greeks.vega << "\n";
    std::cout << "  Theta:   " << greeks.theta << "\n";
    std::cout << "  Rho:     " << greeks.rho << "\n";
}

int main() {
    std::cout << "=== HedgeBot Trading Bot - Option Pricing Engine ===\n\n";
    
    try {
        // Setup market data (typical stock market scenario)
        MarketData market;
        market.spot_price = 100.0;        // S = $100
        market.risk_free_rate = 0.05;     // r = 5%
        market.dividend_yield = 0.02;     // d = 2%
        market.time_to_expiry = 0.25;     // T = 3 months
        market.current_vol = 0.20;        // σ = 20%
        
        std::cout << "Market Conditions:\n";
        std::cout << "  Spot Price:     $" << market.spot_price << "\n";
        std::cout << "  Strike Price:   $100\n";
        std::cout << "  Time to Expiry: " << market.time_to_expiry * 365 << " days\n";
        std::cout << "  Volatility:     " << market.current_vol * 100 << "%\n";
        std::cout << "  Risk-free Rate: " << market.risk_free_rate * 100 << "%\n";
        std::cout << "  Dividend Yield: " << market.dividend_yield * 100 << "%\n\n";
        
        // Test 1: Black-Scholes-Merton Pricer
        std::cout << "================================\n";
        std::cout << "Test 1: Black-Scholes-Merton\n";
        std::cout << "================================\n";
        
        BSM_Pricer bsm_pricer;
        
        OptionData call_option(OptionType::CALL, 100.0, 0.25, "SPY_CALL");
        OptionData put_option(OptionType::PUT, 100.0, 0.25, "SPY_PUT");
        
        double call_price = bsm_pricer.price(call_option, market);
        double put_price = bsm_pricer.price(put_option, market);
        
        std::cout << "Call Option (K=100):\n";
        std::cout << "  Price:  $" << std::fixed << std::setprecision(4) << call_price << "\n";
        std::cout << "  Greeks:\n";
        Greeks call_greeks = bsm_pricer.calculate_greeks(call_option, market);
        print_greeks(call_greeks);
        
        std::cout << "\nPut Option (K=100):\n";
        std::cout << "  Price:  $" << put_price << "\n";
        std::cout << "  Greeks:\n";
        Greeks put_greeks = bsm_pricer.calculate_greeks(put_option, market);
        print_greeks(put_greeks);
        
        // Verify put-call parity: C - P = S*e^(-d*T) - K*e^(-r*T)
        double parity_lhs = call_price - put_price;
        double parity_rhs = market.spot_price * std::exp(-market.dividend_yield * market.time_to_expiry) 
                          - 100.0 * std::exp(-market.risk_free_rate * market.time_to_expiry);
        std::cout << "\nPut-Call Parity Check:\n";
        std::cout << "  C - P =          " << std::fixed << std::setprecision(6) << parity_lhs << "\n";
        std::cout << "  S*e^(-d*T) - K*e^(-r*T) = " << parity_rhs << "\n";
        std::cout << "  Difference:      " << std::abs(parity_lhs - parity_rhs) << " (should be ~0)\n";
        
        // Test 2: Merton Jump-Diffusion Pricer
        std::cout << "\n================================\n";
        std::cout << "Test 2: Merton Jump-Diffusion\n";
        std::cout << "================================\n";
        
        MertonJump_Pricer::Parameters merton_params(
            0.1,   // lambda = 0.1 (10% annual jump probability)
            -0.05, // mu_j = -5% mean jump size
            0.15   // sigma_j = 15% jump volatility
        );
        
        MertonJump_Pricer merton_pricer(merton_params);
        
        double merton_call_price = merton_pricer.price(call_option, market);
        double merton_put_price = merton_pricer.price(put_option, market);
        
        std::cout << "Parameters:\n";
        std::cout << "  Jump Intensity (λ):  " << merton_params.lambda << "\n";
        std::cout << "  Mean Jump (μ_j):     " << merton_params.mu_j << "\n";
        std::cout << "  Jump Volatility (σ_j): " << merton_params.sigma_j << "\n";
        
        std::cout << "\nCall Option (K=100):\n";
        std::cout << "  Price (Merton):  $" << std::fixed << std::setprecision(4) << merton_call_price << "\n";
        std::cout << "  Price (BSM):     $" << call_price << "\n";
        std::cout << "  Difference:      $" << (merton_call_price - call_price) << "\n";
        std::cout << "  Greeks:\n";
        Greeks merton_call_greeks = merton_pricer.calculate_greeks(call_option, market);
        print_greeks(merton_call_greeks);
        
        std::cout << "\nPut Option (K=100):\n";
        std::cout << "  Price (Merton):  $" << std::fixed << std::setprecision(4) << merton_put_price << "\n";
        std::cout << "  Price (BSM):     $" << put_price << "\n";
        std::cout << "  Difference:      $" << (merton_put_price - put_price) << "\n";
        std::cout << "  Greeks:\n";
        Greeks merton_put_greeks = merton_pricer.calculate_greeks(put_option, market);
        print_greeks(merton_put_greeks);
        
        // Test 3: Different strike prices to show vol smile effect
        std::cout << "\n================================\n";
        std::cout << "Test 3: Option Surface (BSM)\n";
        std::cout << "================================\n";
        
        std::vector<double> strikes = {90, 95, 100, 105, 110};
        std::cout << "Strike | Call Price | Put Price | Call Delta | Put Delta\n";
        std::cout << "-------|------------|-----------|------------|----------\n";
        
        std::cout << std::fixed << std::setprecision(2);
        for (double K : strikes) {
            OptionData opt_call(OptionType::CALL, K, 0.25);
            OptionData opt_put(OptionType::PUT, K, 0.25);
            
            double c = bsm_pricer.price(opt_call, market);
            double p = bsm_pricer.price(opt_put, market);
            double delta_c = bsm_pricer.delta(opt_call, market);
            double delta_p = bsm_pricer.delta(opt_put, market);
            
            std::cout << "$" << K << " | $" << c << " | $" << p << " | " 
                     << delta_c << " | " << delta_p << "\n";
        }

        // Test 4: Hedging engine delta/gamma rebalance
        std::cout << "\n================================\n";
        std::cout << "Test 4: Hedging Engine\n";
        std::cout << "================================\n";

        HedgeConfig hedge_cfg;
        hedge_cfg.delta_threshold = 0.5;
        hedge_cfg.gamma_threshold = 0.05;
        hedge_cfg.rebalance_interval_seconds = 3600;
        hedge_cfg.target_delta = 0.0;
        hedge_cfg.target_gamma = 0.0;

        HedgingEngine hedger(hedge_cfg);

        PortfolioGreeksSnapshot portfolio;
        portfolio.underlying_units = 0.0;
        portfolio.option_contracts = 10.0;
        portfolio.option_greeks_per_contract = call_greeks;
        portfolio.last_rebalance_timestamp = 1700000000;

        auto decision = hedger.evaluate_rehedge(portfolio, 1700000900);
        auto pre = decision.current_total_greeks;

        std::cout << "Current total portfolio Greeks:\n";
        std::cout << "  Delta: " << pre.delta << "\n";
        std::cout << "  Gamma: " << pre.gamma << "\n";
        std::cout << "Rehedge decision: " << (decision.should_rehedge ? "YES" : "NO")
                  << " (reason=" << decision.reason << ")\n";

        HedgeInstrumentGreeks gamma_hedge;
        gamma_hedge.enabled = true;
        gamma_hedge.delta_per_unit = 0.30;
        gamma_hedge.gamma_per_unit = 0.10;
        auto plan = hedger.build_delta_gamma_hedge_plan(portfolio, gamma_hedge);

        std::cout << "Suggested hedge trades:\n";
        std::cout << "  Gamma-hedge instrument units: " << plan.gamma_hedge_units << "\n";
        std::cout << "  Underlying units:            " << plan.underlying_trade_units << "\n";
        std::cout << "Projected post-trade Greeks:\n";
        std::cout << "  Delta: " << plan.projected_post_trade_greeks.delta << "\n";
        std::cout << "  Gamma: " << plan.projected_post_trade_greeks.gamma << "\n";

        const double futures_contracts = HedgingEngine::compute_beta_weighted_futures_contracts(
            1.0,
            pre.delta * market.spot_price,
            5000.0,
            50.0);
        std::cout << "Beta-weighted futures contracts (example): " << futures_contracts << "\n";

        // Test 5: Batch backtest from data layout (data/<SYMBOL>/ and data/RATES/)
        std::cout << "\n================================\n";
        std::cout << "Test 5: Batch Backtest From Data Layout\n";
        std::cout << "================================\n";

        const std::string data_root = "data";
        if (std::filesystem::exists(data_root) && std::filesystem::is_directory(data_root)) {
            BacktestConfig bt_cfg;
            bt_cfg.initial_cash = 100000.0;
            bt_cfg.risk_free_rate = 0.02;
            bt_cfg.dividend_yield = 0.0;
            bt_cfg.calibration_window_steps = 60;
            bt_cfg.option_roll_steps = 120;
            bt_cfg.strike_moneyness = 1.0;
            bt_cfg.underlying_symbol = "SPOT";

            HedgeConfig bt_hedge_cfg;
            bt_hedge_cfg.delta_threshold = 0.10;
            bt_hedge_cfg.gamma_threshold = 1.0;
            bt_hedge_cfg.vega_threshold = 1000.0;
            bt_hedge_cfg.rebalance_interval_seconds = 3600;

            ExecutionConfig bt_exec_cfg;
            bt_exec_cfg.fixed_commission = 0.0;
            bt_exec_cfg.commission_rate = 0.0;
            bt_exec_cfg.slippage_bps = 0.0;

            std::vector<HedgeScenario> scenarios;
            const bool heavy_mode = heavy_mode_enabled();
            if (heavy_mode) {
                std::cout << "Heavy sweep mode enabled via HEDGEBOT_HEAVY_SWEEP.\n";
            }

            const std::vector<double> moneyness_grid = heavy_mode
                ? std::vector<double>{0.90, 0.925, 0.95, 0.975, 1.00, 1.025, 1.05, 1.075, 1.10}
                : std::vector<double>{0.95, 1.00, 1.05};
            const std::vector<OptionType> type_grid = {OptionType::CALL, OptionType::PUT};
            const std::vector<double> contracts_grid = heavy_mode
                ? std::vector<double>{-2.0, -1.0, 1.0, 2.0}
                : std::vector<double>{-1.0, 1.0};
            const std::vector<double> delta_grid = heavy_mode
                ? std::vector<double>{0.10, 0.15, 0.20, 0.25}
                : std::vector<double>{0.10, 0.25};
            const std::vector<double> gamma_grid = heavy_mode
                ? std::vector<double>{1.0, 2.0, 3.0}
                : std::vector<double>{1.0, 2.0};
            const std::vector<int> rebalance_grid = heavy_mode
                ? std::vector<int>{3600, 14400, 86400}
                : std::vector<int>{3600, 86400};
            const std::vector<std::size_t> calib_grid = heavy_mode
                ? std::vector<std::size_t>{30, 60, 120}
                : std::vector<std::size_t>{60};
            const std::vector<std::size_t> roll_grid = heavy_mode
                ? std::vector<std::size_t>{60, 120, 240}
                : std::vector<std::size_t>{120};

            for (OptionType opt_type : type_grid) {
                for (double qty : contracts_grid) {
                    for (double mny : moneyness_grid) {
                        for (double dth : delta_grid) {
                            for (double gth : gamma_grid) {
                                for (int rebalance_s : rebalance_grid) {
                                    for (std::size_t calib_w : calib_grid) {
                                        for (std::size_t roll_w : roll_grid) {
                                            BacktestConfig cfg = bt_cfg;
                                            cfg.option_type = opt_type;
                                            cfg.option_contracts = qty;
                                            cfg.strike_moneyness = mny;
                                            cfg.calibration_window_steps = calib_w;
                                            cfg.option_roll_steps = roll_w;

                                            HedgeConfig h = bt_hedge_cfg;
                                            h.delta_threshold = dth;
                                            h.gamma_threshold = gth;
                                            h.rebalance_interval_seconds = rebalance_s;

                                            const std::string side = (qty > 0.0) ? "LONG" : "SHORT";
                                            const std::string name = side + std::string("_") + option_type_name(opt_type)
                                                + "_m" + to_compact(mny)
                                                + "_d" + to_compact(dth)
                                                + "_g" + to_compact(gth)
                                                + "_r" + std::to_string(rebalance_s)
                                                + "_c" + std::to_string(calib_w)
                                                + "_o" + std::to_string(roll_w)
                                                + "_q" + to_compact(qty, 0);

                                            scenarios.push_back({name, cfg, h});
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            std::cout << "Total scenarios to evaluate: " << scenarios.size() << "\n";

            Backtester backtester(bt_cfg);

            std::vector<ScenarioBatchResult> sweep_results;
            sweep_results.reserve(scenarios.size());

            for (const auto& scenario : scenarios) {
                ScenarioBatchResult one;
                one.name = scenario.name;
                one.backtest = scenario.backtest;
                one.hedge = scenario.hedge;
                backtester.set_config(scenario.backtest);
                one.batch = backtester.run_batch_from_layout(data_root, scenario.hedge, bt_exec_cfg, "RATES");
                one.avg_sharpe = compute_avg_sharpe(one.batch);
                sweep_results.push_back(one);
            }

            std::cout << "Strategy + hedge parameter sweep:\n";
            std::cout << "  Scenario | Success | AvgPnL | AvgSharpe | Instruments\n";
            std::cout << "  ---------|---------|--------|-----------|------------\n";

            double best_pnl = -std::numeric_limits<double>::infinity();
            std::size_t best_idx = 0;
            for (std::size_t i = 0; i < sweep_results.size(); ++i) {
                const auto& one = sweep_results[i];
                std::cout << "  " << one.name
                          << " | " << (one.batch.success ? "YES" : "NO")
                          << " | " << std::fixed << std::setprecision(4) << one.batch.average_total_pnl
                          << " | " << one.avg_sharpe
                          << " | " << one.batch.instrument_results.size() << "\n";

                if (one.batch.success && one.batch.average_total_pnl > best_pnl) {
                    best_pnl = one.batch.average_total_pnl;
                    best_idx = i;
                }
            }

            if (!sweep_results.empty() && sweep_results[best_idx].batch.success) {
                const auto& best = sweep_results[best_idx];
                std::cout << "\nRecommended scenario: " << best.name
                          << " (AvgPnL=" << best.batch.average_total_pnl
                          << ", AvgSharpe=" << best.avg_sharpe << ")\n";

                std::cout << "Best-scenario instrument details:\n";
                for (const auto& one : best.batch.instrument_results) {
                    std::cout << "  - " << one.symbol << ": "
                              << (one.result.success ? "OK" : "FAIL")
                              << ", msg=" << one.result.message
                              << ", PnL=" << one.result.total_pnl
                              << ", Sharpe=" << one.result.sharpe
                              << ", MaxDD=" << one.result.max_drawdown_pct << "\n";
                }

                // Robustness check under non-zero execution costs.
                std::cout << "\nRobustness test (with execution costs):\n";
                ExecutionConfig realistic_exec = bt_exec_cfg;
                realistic_exec.fixed_commission = 0.25;
                realistic_exec.commission_rate = 0.0005;
                realistic_exec.slippage_bps = 1.0;

                backtester.set_config(best.backtest);
                const auto best_realistic = backtester.run_batch_from_layout(data_root, best.hedge, realistic_exec, "RATES");
                const double best_realistic_sharpe = compute_avg_sharpe(best_realistic);
                std::cout << "  AvgPnL (realistic costs): " << best_realistic.average_total_pnl
                          << ", AvgSharpe: " << best_realistic_sharpe << "\n";

                const std::filesystem::path results_dir = std::filesystem::path("results");
                std::filesystem::create_directories(results_dir);
                const std::filesystem::path sweep_csv = results_dir / "strategy_sweep.csv";
                const std::filesystem::path best_yaml = results_dir / "best_strategy.yaml";
                save_sweep_csv(sweep_csv, sweep_results);
                save_best_config_yaml(best_yaml, best, data_root);

                std::cout << "\nSaved outputs:\n";
                std::cout << "  - " << sweep_csv.string() << "\n";
                std::cout << "  - " << best_yaml.string() << "\n";
            } else {
                std::cout << "Batch backtest unavailable across all scenarios.\n";
            }
        } else {
            std::cout << "Data folder not found, skipping batch backtest demo.\n";
        }
        
        std::cout << "\n=== Initialization Successful ===\n";
        std::cout << "Project structure and basic pricing engine ready!\n";
        
        return 0;
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
