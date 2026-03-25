#pragma once

#include "backtester.hpp"
#include "broker_simulator.hpp"
#include "bsm_pricer.hpp"
#include "hedging_engine.hpp"
#include "market_data.hpp"

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace hedgebot {

struct LiveBotConfig {
    std::string mode;                  // paper | live-dryrun
    std::string symbol;
    std::string data_root;
    std::string strategy_file;
    double initial_cash;
    std::size_t max_steps;
    bool use_best_strategy_file;

    // Placeholder credentials for later real broker/data SDK integration.
    std::string broker_api_key;
    std::string broker_api_secret;
    std::string data_api_key;

    LiveBotConfig()
        : mode("paper"),
          symbol("SPY"),
          data_root("data"),
          strategy_file("results/best_strategy.yaml"),
          initial_cash(100000.0),
          max_steps(240),
          use_best_strategy_file(true) {}
};

struct ReplayTick {
    std::int64_t timestamp;
    double spot;
    double rate;
    double implied_vol;
};

class DotEnvLoader {
public:
    static std::unordered_map<std::string, std::string> load(const std::string& path);
};

class StrategyFileLoader {
public:
    static bool load_best_strategy(
        const std::string& path,
        BacktestConfig& out_backtest,
        HedgeConfig& out_hedge,
        std::string& out_scenario_name);
};

class IOrderExecutor {
public:
    virtual ~IOrderExecutor() = default;
    virtual FillReport place_order(const OrderRequest& req) = 0;
    virtual void mark_price(const std::string& symbol, double price) = 0;
    virtual double position_quantity(const std::string& symbol) const = 0;
    virtual double total_equity() const = 0;
};

class PaperOrderExecutor : public IOrderExecutor {
public:
    PaperOrderExecutor(const ExecutionConfig& exec_cfg, double initial_cash);

    FillReport place_order(const OrderRequest& req) override;
    void mark_price(const std::string& symbol, double price) override;
    double position_quantity(const std::string& symbol) const override;
    double total_equity() const override;

private:
    BrokerSimulator broker_;
};

class DryRunLiveOrderExecutor : public IOrderExecutor {
public:
    explicit DryRunLiveOrderExecutor(double initial_cash);

    FillReport place_order(const OrderRequest& req) override;
    void mark_price(const std::string& symbol, double price) override;
    double position_quantity(const std::string& symbol) const override;
    double total_equity() const override;

private:
    double cash_;
    std::unordered_map<std::string, double> qty_;
    std::unordered_map<std::string, double> marks_;
};

class ReplayMarketFeed {
public:
    ReplayMarketFeed(const std::string& data_root, const std::string& symbol, double fallback_rate);

    const std::vector<ReplayTick>& ticks() const;

private:
    std::vector<ReplayTick> ticks_;
};

class LiveTradingBot {
public:
    explicit LiveTradingBot(const LiveBotConfig& cfg);
    bool run();

private:
    LiveBotConfig cfg_;
};

} // namespace hedgebot
