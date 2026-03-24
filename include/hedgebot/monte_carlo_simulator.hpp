#pragma once

#include "option_data.hpp"
#include "pricer_base.hpp"

#include <cstdint>
#include <vector>

namespace hedgebot {

struct MonteCarloConfig {
    int num_paths;
    int num_steps;
    double dt;
    std::uint64_t seed;
    bool antithetic;

    MonteCarloConfig()
        : num_paths(10000), num_steps(252), dt(1.0 / 252.0), seed(42), antithetic(true) {}
};

struct DistributionSummary {
    double mean;
    double stddev;
    double p05;
    double p50;
    double p95;

    DistributionSummary() : mean(0.0), stddev(0.0), p05(0.0), p50(0.0), p95(0.0) {}
};

class MonteCarloSimulator {
public:
    explicit MonteCarloSimulator(const MonteCarloConfig& config = MonteCarloConfig());

    void set_config(const MonteCarloConfig& config);
    const MonteCarloConfig& config() const;

    std::vector<std::vector<double>> generate_gbm_paths(
        double s0,
        double mu,
        double sigma) const;

    std::vector<double> generate_gbm_terminal_prices(
        double s0,
        double mu,
        double sigma,
        double time_horizon_years) const;

    std::vector<double> simulate_discounted_option_pnl(
        const OptionData& option,
        const OptionPricer& pricer,
        const MarketData& market,
        double mu_under_q) const;

    static DistributionSummary summarize_distribution(const std::vector<double>& samples);

private:
    MonteCarloConfig config_;
};

} // namespace hedgebot
