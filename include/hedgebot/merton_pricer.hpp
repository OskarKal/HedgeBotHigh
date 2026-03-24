#pragma once

#include "pricer_base.hpp"
#include <array>

namespace hedgebot {

// Merton Jump-Diffusion pricer (Merton 1976)
// dS_t = rS_t dt + σ S_t dW_t + S_{t-} dJ_t
// where J_t is a Poisson process with intensity λ and lognormal jump sizes
class MertonJump_Pricer : public OptionPricer {
public:
    // Constructor with model parameters
    struct Parameters {
        double lambda;      // Jump intensity (jumps per year)
        double mu_j;        // Mean log-jump size (lognormal)
        double sigma_j;     // Std dev of log-jump size
        
        Parameters(double l = 0.1, double m = 0.0, double s = 0.1)
            : lambda(l), mu_j(m), sigma_j(s) {}
    };
    
    MertonJump_Pricer(const Parameters& params = Parameters());
    virtual ~MertonJump_Pricer() = default;
    
    // Price using Merton's series expansion (summing over jump regimes)
    double price(const OptionData& option, const MarketData& market) const override;
    
    // Calculate Greeks numerically
    Greeks calculate_greeks(const OptionData& option, const MarketData& market) const override;
    
    // Parameter accessors
    void set_parameters(const Parameters& params) { params_ = params; }
    const Parameters& get_parameters() const { return params_; }
    
    std::string name() const override { return "Merton Jump-Diffusion"; }
    
private:
    Parameters params_;
    static constexpr int MAX_JUMPS = 50;  // Maximum terms in series expansion
    static constexpr double JUMP_CUTOFF = 1e-10;  // Threshold for series convergence
    
    // Helper: price for pure diffusion component (given number of jumps)
    // Used in Merton's series formula
    double price_with_k_jumps(const OptionData& option, const MarketData& market, int k) const;
    
    // Expected return adjustment due to jumps
    double adjusted_drift_for_k_jumps(int k) const;
    
    // Poisson probability P(N_T = k)
    double poisson_probability(int k) const;
    
    // Greeks calculated by numerical differentiation
    double numerical_derivative(const OptionData& option, const MarketData& market,
                               std::function<double(const MarketData&)> pricer_func) const;
};

} // namespace hedgebot
