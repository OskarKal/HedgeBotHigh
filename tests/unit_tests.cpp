#include "hedgebot/bsm_pricer.hpp"
#include "hedgebot/backtester.hpp"
#include "hedgebot/broker_simulator.hpp"
#include "hedgebot/hedging_engine.hpp"
#include "hedgebot/market_data.hpp"
#include "hedgebot/merton_pricer.hpp"
#include "hedgebot/monte_carlo_simulator.hpp"
#include "hedgebot/model_calibrator.hpp"
#include "hedgebot/risk_manager.hpp"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <stdexcept>

using namespace hedgebot;

namespace {

void expect(bool condition, const char* message) {
    if (!condition) {
        throw std::runtime_error(message);
    }
}

} // namespace

int main() {
    std::cout << "Running unit tests...\n";
    
    // Test 1: BSM Pricer instantiation
    {
        std::cout << "Test 1: BSM Pricer instantiation... ";
        BSM_Pricer pricer;
        expect(pricer.name() == "Black-Scholes-Merton", "Unexpected BSM pricer name");
        std::cout << "PASSED\n";
    }
    
    // Test 2: Merton Pricer instantiation
    {
        std::cout << "Test 2: Merton Pricer instantiation... ";
        MertonJump_Pricer::Parameters params(0.1, -0.05, 0.15);
        MertonJump_Pricer pricer(params);
        expect(pricer.name() == "Merton Jump-Diffusion", "Unexpected Merton pricer name");
        std::cout << "PASSED\n";
    }
    
    // Test 3: Option intrinsic value
    {
        std::cout << "Test 3: Option intrinsic value... ";
        OptionData call(OptionType::CALL, 100.0, 0.25);
        OptionData put(OptionType::PUT, 100.0, 0.25);
        
        double call_intrinsic = call.intrinsic_value(110.0);
        double put_intrinsic = put.intrinsic_value(90.0);
        
        expect(std::abs(call_intrinsic - 10.0) < 1e-10, "Call intrinsic mismatch");
        expect(std::abs(put_intrinsic - 10.0) < 1e-10, "Put intrinsic mismatch");
        std::cout << "PASSED\n";
    }
    
    // Test 4: BSM prices are positive
    {
        std::cout << "Test 4: BSM prices positive... ";
        MarketData market;
        market.spot_price = 100.0;
        market.risk_free_rate = 0.05;
        market.dividend_yield = 0.02;
        market.time_to_expiry = 0.25;
        market.current_vol = 0.20;
        
        OptionData call(OptionType::CALL, 100.0, 0.25);
        OptionData put(OptionType::PUT, 100.0, 0.25);
        
        BSM_Pricer pricer;
        double call_price = pricer.price(call, market);
        double put_price = pricer.price(put, market);
        
        expect(call_price > 0, "Call price must be positive");
        expect(put_price > 0, "Put price must be positive");
        std::cout << "PASSED\n";
    }

    // Test 5: MarketDataFetcher loads CSV price data and sorts rows
    {
        std::cout << "Test 5: MarketDataFetcher CSV load... ";

        const std::string path = "/tmp/hedgebot_prices_test.csv";
        {
            std::ofstream out(path);
            out << "timestamp,close\n";
            out << "1700000600,101.5\n";
            out << "1700000000,100.0\n";
        }

        MarketDataFetcher fetcher;
        auto prices = fetcher.load_price_data(path);
        expect(prices.size() == 2, "Unexpected number of loaded prices");
        expect(prices[0].timestamp == 1700000000, "Price rows are not sorted");
        expect(std::abs(prices[0].close - 100.0) < 1e-12, "First close value mismatch");
        expect(prices[1].timestamp == 1700000600, "Second timestamp mismatch");
        expect(std::abs(prices[1].close - 101.5) < 1e-12, "Second close value mismatch");

        std::remove(path.c_str());
        std::cout << "PASSED\n";
    }

    // Test 6: DataNormalizer fills missing intervals with linear interpolation
    {
        std::cout << "Test 6: DataNormalizer interpolation... ";

        std::vector<PriceBar> prices = {
            {1000, 100.0},
            {1180, 109.0} // gap is 180 seconds; expected step is 60 seconds.
        };

        DataNormalizer normalizer;
        auto filled = normalizer.fill_missing_prices_linear(prices, 60);
        expect(filled.size() == 4, "Unexpected interpolated series size");
        expect(filled[1].timestamp == 1060, "Interpolated timestamp[1] mismatch");
        expect(std::abs(filled[1].close - 103.0) < 1e-12, "Interpolated close[1] mismatch");
        expect(filled[2].timestamp == 1120, "Interpolated timestamp[2] mismatch");
        expect(std::abs(filled[2].close - 106.0) < 1e-12, "Interpolated close[2] mismatch");

        std::cout << "PASSED\n";
    }

    // Test 7: ModelCalibrator recovers BSM volatility from synthetic prices
    {
        std::cout << "Test 7: BSM calibration on synthetic data... ";

        const double true_sigma = 0.24;
        MarketData base_market;
        base_market.spot_price = 100.0;
        base_market.risk_free_rate = 0.03;
        base_market.dividend_yield = 0.01;
        base_market.time_to_expiry = 0.25;
        base_market.current_vol = true_sigma;

        BSM_Pricer bsm;
        std::vector<OptionQuote> surface;
        for (double strike : {90.0, 95.0, 100.0, 105.0, 110.0}) {
            OptionData c(OptionType::CALL, strike, 0.25);
            const double mid = bsm.price(c, base_market);
            surface.push_back({OptionType::CALL, strike, 0.25, mid, mid, true_sigma});
        }

        ModelCalibrator calibrator;
        const auto result = calibrator.calibrate_bsm_vol(surface, base_market, 0.05, 0.80, 100);
        expect(result.summary.success, "BSM calibration failed");
        expect(std::abs(result.sigma - true_sigma) < 0.02, "BSM sigma not recovered within tolerance");

        std::cout << "PASSED\n";
    }

    // Test 8: ModelCalibrator finds Merton parameters near synthetic source values
    {
        std::cout << "Test 8: Merton calibration on synthetic data... ";

        MarketData base_market;
        base_market.spot_price = 100.0;
        base_market.risk_free_rate = 0.02;
        base_market.dividend_yield = 0.00;
        base_market.time_to_expiry = 0.25;
        base_market.current_vol = 0.20;

        const MertonJump_Pricer::Parameters true_params(0.15, -0.08, 0.20);
        MertonJump_Pricer true_model(true_params);
        std::vector<OptionQuote> surface;
        for (double strike : {92.0, 96.0, 100.0, 104.0, 108.0}) {
            OptionData c(OptionType::CALL, strike, 0.25);
            const double mid = true_model.price(c, base_market);
            surface.push_back({OptionType::CALL, strike, 0.25, mid, mid, 0.20});
        }

        ModelCalibrator calibrator;
        const auto result = calibrator.calibrate_merton_params(
            surface,
            base_market,
            0.05,
            0.30,
            -0.20,
            0.00,
            0.10,
            0.35,
            8,
            8,
            8);

        expect(result.summary.success, "Merton calibration failed");
        expect(std::abs(result.params.lambda - true_params.lambda) < 0.08, "Merton lambda not recovered");
        expect(std::abs(result.params.mu_j - true_params.mu_j) < 0.08, "Merton mu_j not recovered");
        expect(std::abs(result.params.sigma_j - true_params.sigma_j) < 0.10, "Merton sigma_j not recovered");

        std::cout << "PASSED\n";
    }

    // Test 9: HedgingEngine threshold trigger on delta/gamma breach
    {
        std::cout << "Test 9: HedgingEngine threshold trigger... ";

        HedgeConfig cfg;
        cfg.delta_threshold = 0.5;
        cfg.gamma_threshold = 0.05;
        cfg.vega_threshold = 1000.0;
        cfg.rebalance_interval_seconds = 3600;

        HedgingEngine engine(cfg);

        PortfolioGreeksSnapshot snapshot;
        snapshot.underlying_units = 0.0;
        snapshot.option_contracts = 10.0;
        snapshot.option_greeks_per_contract.delta = 0.2; // total delta = 2.0
        snapshot.option_greeks_per_contract.gamma = 0.01; // total gamma = 0.1
        snapshot.last_rebalance_timestamp = 1000;

        const auto decision = engine.evaluate_rehedge(snapshot, 1100);
        expect(decision.should_rehedge, "Expected rehedge due to threshold breach");
        expect(decision.due_to_threshold, "Expected threshold trigger flag");
        expect(!decision.due_to_time, "Did not expect time trigger");

        std::cout << "PASSED\n";
    }

    // Test 10: HedgingEngine plan neutralizes delta and gamma with hedge instrument
    {
        std::cout << "Test 10: HedgingEngine delta-gamma plan... ";

        HedgeConfig cfg;
        cfg.target_delta = 0.0;
        cfg.target_gamma = 0.0;
        cfg.max_underlying_trade = 1e6;

        HedgingEngine engine(cfg);

        PortfolioGreeksSnapshot snapshot;
        snapshot.underlying_units = 0.0;
        snapshot.option_contracts = 10.0;
        snapshot.option_greeks_per_contract.delta = 0.5;  // total delta = 5.0
        snapshot.option_greeks_per_contract.gamma = 0.05; // total gamma = 0.5

        HedgeInstrumentGreeks gamma_hedge;
        gamma_hedge.enabled = true;
        gamma_hedge.delta_per_unit = 0.3;
        gamma_hedge.gamma_per_unit = 0.1;

        const auto plan = engine.build_delta_gamma_hedge_plan(snapshot, gamma_hedge);

        expect(std::abs(plan.gamma_hedge_units + 5.0) < 1e-9, "Unexpected gamma hedge size");
        expect(std::abs(plan.underlying_trade_units - (-3.5)) < 1e-9, "Unexpected underlying hedge size");
        expect(std::abs(plan.projected_post_trade_greeks.gamma) < 1e-9, "Post-trade gamma should be neutral");
        expect(std::abs(plan.projected_post_trade_greeks.delta) < 1e-9, "Post-trade delta should be neutral");

        std::cout << "PASSED\n";
    }

    // Test 11: BrokerSimulator tracks position and unrealized PnL
    {
        std::cout << "Test 11: BrokerSimulator unrealized PnL... ";

        ExecutionConfig exec_cfg;
        exec_cfg.fixed_commission = 0.0;
        exec_cfg.commission_rate = 0.0;
        exec_cfg.slippage_bps = 0.0;

        BrokerSimulator broker(exec_cfg, 10000.0);
        const auto fill = broker.send_order({"SPY", 10.0, 100.0, 1700000000});
        expect(fill.success, "Expected fill success");

        const auto pos = broker.get_position("SPY");
        expect(std::abs(pos.quantity - 10.0) < 1e-12, "Unexpected position quantity");
        expect(std::abs(pos.average_price - 100.0) < 1e-12, "Unexpected average price");

        broker.mark_price("SPY", 103.0);
        expect(std::abs(broker.get_unrealized_pnl("SPY") - 30.0) < 1e-12, "Unexpected unrealized PnL");

        std::cout << "PASSED\n";
    }

    // Test 12: BrokerSimulator realizes PnL on close and applies costs
    {
        std::cout << "Test 12: BrokerSimulator realized PnL + costs... ";

        ExecutionConfig exec_cfg;
        exec_cfg.fixed_commission = 1.0;
        exec_cfg.commission_rate = 0.001; // 0.1%
        exec_cfg.slippage_bps = 0.0;

        BrokerSimulator broker(exec_cfg, 0.0);

        const auto buy = broker.send_order({"SPY", 10.0, 100.0, 1700000000});
        expect(buy.success, "Buy fill failed");
        const auto sell = broker.send_order({"SPY", -10.0, 101.0, 1700000100});
        expect(sell.success, "Sell fill failed");

        // Realized trading PnL = (101 - 100) * 10 = 10
        expect(std::abs(broker.get_total_realized_pnl() - 10.0) < 1e-12, "Unexpected realized PnL");

        // Commission buy = 1 + 0.001*1000 = 2, sell = 1 + 0.001*1010 = 2.01 => total 4.01
        // Net cash after round trip should be realized pnl - commissions = 5.99
        expect(std::abs(broker.get_cash() - 5.99) < 1e-9, "Unexpected cash after round trip");

        std::cout << "PASSED\n";
    }

    // Test 13: RiskManager historical VaR/ES from sample PnL
    {
        std::cout << "Test 13: RiskManager VaR/ES... ";

        std::vector<double> pnl = {-10.0, -5.0, -3.0, 2.0, 8.0};
        const auto metrics = RiskManager::historical_var_es(pnl, 0.80);

        // Losses sorted: -8, -2, 3, 5, 10 -> VaR_80 = losses[3] = 5.
        expect(std::abs(metrics.var_value - 5.0) < 1e-12, "Unexpected VaR value");
        expect(std::abs(metrics.es_value - 7.5) < 1e-12, "Unexpected ES value");

        std::cout << "PASSED\n";
    }

    // Test 14: RiskManager detects leverage and drawdown breaches
    {
        std::cout << "Test 14: RiskManager limit breaches... ";

        RiskConfig cfg;
        cfg.max_abs_delta = 0.5;
        cfg.max_abs_gamma = 1.0;
        cfg.max_abs_vega = 1e6;
        cfg.max_leverage = 2.0;
        cfg.max_drawdown_pct = 0.10;
        cfg.stop_loss_pct = 0.05;

        RiskManager rm(cfg);
        Greeks g;
        g.delta = 1.0; // breach
        g.gamma = 0.2;
        g.vega = 100.0;

        const auto decision = rm.evaluate(
            g,
            300000.0,                 // gross notional
            100000.0,                 // equity => leverage 3.0 (breach)
            120000.0,                 // peak => drawdown 16.67% (breach)
            {-100.0, 20.0, -50.0},
            -6000.0,                  // intraday pnl
            100000.0);                // stop loss at -5000 (breach)

        expect(!decision.within_limits, "Expected limits breach");
        expect(decision.breach_delta, "Expected delta breach");
        expect(decision.breach_leverage, "Expected leverage breach");
        expect(decision.breach_drawdown, "Expected drawdown breach");
        expect(decision.stop_loss_triggered, "Expected stop-loss trigger");
        expect(decision.circuit_breaker_triggered, "Expected circuit breaker trigger");

        std::cout << "PASSED\n";
    }

    // Test 15: MonteCarloSimulator path dimensions and positivity
    {
        std::cout << "Test 15: MonteCarlo path generation... ";

        MonteCarloConfig cfg;
        cfg.num_paths = 200;
        cfg.num_steps = 50;
        cfg.dt = 1.0 / 252.0;
        cfg.seed = 123;
        cfg.antithetic = true;

        MonteCarloSimulator mc(cfg);
        const auto paths = mc.generate_gbm_paths(100.0, 0.05, 0.2);
        expect(paths.size() == 200, "Unexpected number of MC paths");
        expect(paths[0].size() == 51, "Unexpected path length");
        expect(paths[0][0] == 100.0, "Path should start at S0");
        for (const auto& p : paths) {
            for (double s : p) {
                expect(s > 0.0, "GBM path should remain positive");
            }
        }

        std::cout << "PASSED\n";
    }

    // Test 16: MonteCarlo terminal mean is close to GBM expectation
    {
        std::cout << "Test 16: MonteCarlo terminal expectation... ";

        MonteCarloConfig cfg;
        cfg.num_paths = 20000;
        cfg.num_steps = 252;
        cfg.dt = 1.0 / 252.0;
        cfg.seed = 42;
        cfg.antithetic = true;

        MonteCarloSimulator mc(cfg);
        const auto terminal = mc.generate_gbm_terminal_prices(100.0, 0.03, 0.2, 1.0);
        const auto summary = MonteCarloSimulator::summarize_distribution(terminal);

        const double expected = 100.0 * std::exp(0.03 * 1.0);
        expect(std::abs(summary.mean - expected) < 1.0, "Terminal mean too far from GBM expectation");

        std::cout << "PASSED\n";
    }

    // Test 17: Risk-neutral discounted option PnL has near-zero mean
    {
        std::cout << "Test 17: MonteCarlo option PnL neutrality... ";

        MonteCarloConfig cfg;
        cfg.num_paths = 15000;
        cfg.num_steps = 252;
        cfg.dt = 1.0 / 252.0;
        cfg.seed = 777;
        cfg.antithetic = true;

        MonteCarloSimulator mc(cfg);

        MarketData market;
        market.spot_price = 100.0;
        market.risk_free_rate = 0.02;
        market.dividend_yield = 0.0;
        market.time_to_expiry = 1.0;
        market.current_vol = 0.2;

        OptionData call(OptionType::CALL, 100.0, 1.0);
        BSM_Pricer bsm;

        const auto pnl = mc.simulate_discounted_option_pnl(call, bsm, market, market.risk_free_rate);
        const auto summary = MonteCarloSimulator::summarize_distribution(pnl);
        expect(std::abs(summary.mean) < 0.35, "Discounted option PnL mean should be near zero under Q");

        std::cout << "PASSED\n";
    }

    // Test 18: Backtester runs end-to-end and returns metrics
    {
        std::cout << "Test 18: Backtester end-to-end run... ";

        std::vector<PriceBar> prices;
        prices.reserve(300);
        std::int64_t ts = 1700000000;
        for (int i = 0; i < 300; ++i) {
            // Smoothly trending synthetic price path.
            const double s = 100.0 + 0.02 * static_cast<double>(i) + 0.5 * std::sin(0.05 * static_cast<double>(i));
            prices.push_back({ts, s});
            ts += 60;
        }

        std::vector<OptionQuote> surface = {
            {OptionType::CALL, 95.0, 30.0 / 365.0, 5.8, 6.0, 0.20},
            {OptionType::CALL, 100.0, 30.0 / 365.0, 2.8, 3.0, 0.19},
            {OptionType::CALL, 105.0, 30.0 / 365.0, 1.1, 1.3, 0.21},
            {OptionType::PUT, 95.0, 30.0 / 365.0, 0.7, 0.9, 0.22},
            {OptionType::PUT, 100.0, 30.0 / 365.0, 2.3, 2.5, 0.20},
            {OptionType::PUT, 105.0, 30.0 / 365.0, 5.5, 5.8, 0.23}
        };

        std::vector<RatePoint> rates = {
            {1700000000, 0.02},
            {1700000000 + 86400, 0.021}
        };

        BacktestConfig bt_cfg;
        bt_cfg.initial_cash = 100000.0;
        bt_cfg.risk_free_rate = 0.02;
        bt_cfg.dividend_yield = 0.0;
        bt_cfg.initial_vol_guess = 0.2;
        bt_cfg.calibration_window_steps = 50;
        bt_cfg.option_roll_steps = 120;
        bt_cfg.strike_moneyness = 1.0;
        bt_cfg.underlying_symbol = "SPOT";

        HedgeConfig hedge_cfg;
        hedge_cfg.delta_threshold = 0.1;
        hedge_cfg.gamma_threshold = 1.0;
        hedge_cfg.vega_threshold = 1000.0;
        hedge_cfg.rebalance_interval_seconds = 600;

        ExecutionConfig exec_cfg;
        exec_cfg.fixed_commission = 0.0;
        exec_cfg.commission_rate = 0.0;
        exec_cfg.slippage_bps = 0.0;

        Backtester bt(bt_cfg);
        const auto res = bt.run_from_data(prices, surface, rates, hedge_cfg, exec_cfg);

        expect(res.success, "Backtest should succeed");
        expect(!res.equity_curve.empty(), "Backtest should produce equity curve");
        expect(res.equity_curve.size() == res.pnl_curve.size(), "Equity/PnL curve size mismatch");

        std::cout << "PASSED\n";
    }
    
    std::cout << "\nAll tests passed!\n";
    return 0;
}
