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

using namespace hedgebot;

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
            bt_hedge_cfg.rebalance_interval_seconds = 600;

            ExecutionConfig bt_exec_cfg;
            bt_exec_cfg.fixed_commission = 0.0;
            bt_exec_cfg.commission_rate = 0.0;
            bt_exec_cfg.slippage_bps = 0.0;

            Backtester backtester(bt_cfg);
            auto batch = backtester.run_batch_from_layout(data_root, bt_hedge_cfg, bt_exec_cfg, "RATES");

            if (!batch.success) {
                std::cout << "Batch backtest unavailable: " << batch.message << "\n";
            }

            std::cout << "Batch backtest details:\n";
            std::cout << "  Instruments seen: " << batch.instrument_results.size() << "\n";
            if (batch.success) {
                std::cout << "  Average PnL: " << std::fixed << std::setprecision(4)
                          << batch.average_total_pnl << "\n";
            }

            for (const auto& one : batch.instrument_results) {
                std::cout << "  - " << one.symbol << ": "
                          << (one.result.success ? "OK" : "FAIL")
                          << ", msg=" << one.result.message
                          << ", PnL=" << one.result.total_pnl
                          << ", Sharpe=" << one.result.sharpe
                          << ", MaxDD=" << one.result.max_drawdown_pct << "\n";
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
