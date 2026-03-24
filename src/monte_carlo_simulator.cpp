#include "hedgebot/monte_carlo_simulator.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>
#include <stdexcept>

namespace hedgebot {

namespace {

double percentile_sorted(const std::vector<double>& sorted, double p) {
    if (sorted.empty()) {
        return 0.0;
    }
    if (p <= 0.0) {
        return sorted.front();
    }
    if (p >= 1.0) {
        return sorted.back();
    }

    const double idx = p * static_cast<double>(sorted.size() - 1);
    const std::size_t lo = static_cast<std::size_t>(std::floor(idx));
    const std::size_t hi = static_cast<std::size_t>(std::ceil(idx));
    const double w = idx - static_cast<double>(lo);
    return sorted[lo] * (1.0 - w) + sorted[hi] * w;
}

} // namespace

MonteCarloSimulator::MonteCarloSimulator(const MonteCarloConfig& config) : config_(config) {}

void MonteCarloSimulator::set_config(const MonteCarloConfig& config) {
    config_ = config;
}

const MonteCarloConfig& MonteCarloSimulator::config() const {
    return config_;
}

std::vector<std::vector<double>> MonteCarloSimulator::generate_gbm_paths(
    double s0,
    double mu,
    double sigma) const {
    if (s0 <= 0.0) {
        throw std::invalid_argument("generate_gbm_paths: s0 must be positive");
    }
    if (sigma < 0.0) {
        throw std::invalid_argument("generate_gbm_paths: sigma must be non-negative");
    }
    if (config_.num_paths <= 0 || config_.num_steps <= 0 || config_.dt <= 0.0) {
        throw std::invalid_argument("generate_gbm_paths: invalid Monte Carlo config");
    }

    std::mt19937_64 rng(config_.seed);
    std::normal_distribution<double> nd(0.0, 1.0);

    std::vector<std::vector<double>> paths(
        static_cast<std::size_t>(config_.num_paths),
        std::vector<double>(static_cast<std::size_t>(config_.num_steps + 1), s0));

    const double drift = (mu - 0.5 * sigma * sigma) * config_.dt;
    const double diffusion = sigma * std::sqrt(config_.dt);

    for (int i = 0; i < config_.num_paths; ++i) {
        for (int t = 1; t <= config_.num_steps; ++t) {
            double z = nd(rng);
            if (config_.antithetic && (i % 2 == 1)) {
                z = -z;
            }
            const double prev = paths[static_cast<std::size_t>(i)][static_cast<std::size_t>(t - 1)];
            const double next = prev * std::exp(drift + diffusion * z);
            paths[static_cast<std::size_t>(i)][static_cast<std::size_t>(t)] = next;
        }
    }

    return paths;
}

std::vector<double> MonteCarloSimulator::generate_gbm_terminal_prices(
    double s0,
    double mu,
    double sigma,
    double time_horizon_years) const {
    if (time_horizon_years <= 0.0) {
        throw std::invalid_argument("generate_gbm_terminal_prices: time_horizon_years must be positive");
    }

    MonteCarloConfig tmp = config_;
    tmp.num_steps = std::max(1, static_cast<int>(std::round(time_horizon_years / config_.dt)));

    MonteCarloSimulator sim(tmp);
    const auto paths = sim.generate_gbm_paths(s0, mu, sigma);

    std::vector<double> terminal;
    terminal.reserve(paths.size());
    for (const auto& p : paths) {
        terminal.push_back(p.back());
    }
    return terminal;
}

std::vector<double> MonteCarloSimulator::simulate_discounted_option_pnl(
    const OptionData& option,
    const OptionPricer& pricer,
    const MarketData& market,
    double mu_under_q) const {
    if (market.time_to_expiry <= 0.0) {
        throw std::invalid_argument("simulate_discounted_option_pnl: market.time_to_expiry must be positive");
    }

    const double initial_price = pricer.price(option, market);
    const auto terminal_prices = generate_gbm_terminal_prices(
        market.spot_price,
        mu_under_q,
        market.current_vol,
        market.time_to_expiry);

    std::vector<double> pnl;
    pnl.reserve(terminal_prices.size());

    for (double st : terminal_prices) {
        const double discounted_payoff = std::exp(-market.risk_free_rate * market.time_to_expiry) * option.payoff(st);
        pnl.push_back(discounted_payoff - initial_price);
    }

    return pnl;
}

DistributionSummary MonteCarloSimulator::summarize_distribution(const std::vector<double>& samples) {
    if (samples.empty()) {
        throw std::invalid_argument("summarize_distribution: samples cannot be empty");
    }

    DistributionSummary out;
    out.mean = std::accumulate(samples.begin(), samples.end(), 0.0) / static_cast<double>(samples.size());

    double var = 0.0;
    for (double x : samples) {
        const double d = x - out.mean;
        var += d * d;
    }
    var /= static_cast<double>(samples.size());
    out.stddev = std::sqrt(var);

    std::vector<double> sorted = samples;
    std::sort(sorted.begin(), sorted.end());
    out.p05 = percentile_sorted(sorted, 0.05);
    out.p50 = percentile_sorted(sorted, 0.50);
    out.p95 = percentile_sorted(sorted, 0.95);
    return out;
}

} // namespace hedgebot
