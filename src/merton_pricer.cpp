#include "hedgebot/merton_pricer.hpp"
#include "hedgebot/bsm_pricer.hpp"
#include <cmath>
#include <functional>

namespace hedgebot {

MertonJump_Pricer::MertonJump_Pricer(const Parameters& params)
    : params_(params) {}

double MertonJump_Pricer::poisson_probability(int k) const {
    if (k < 0) return 0.0;
    
    double lambda_T = params_.lambda;  // Assuming time interval of 1 year
    
    // P(N_T = k) = (λT)^k * e^(-λT) / k!
    double numerator = std::pow(lambda_T, k);
    double denominator = 1.0;
    for (int i = 1; i <= k; ++i) {
        denominator *= i;
    }
    
    return numerator * std::exp(-lambda_T) / denominator;
}

double MertonJump_Pricer::adjusted_drift_for_k_jumps(int k) const {
    // Expected log-return from jumps: λ * E[ln(1 + J)]
    // For lognormal jumps: E[ln(1 + J)] = μ_j + 0.5 * σ_j^2 - 1
    double expected_log_return = params_.lambda * (params_.mu_j + 0.5 * params_.sigma_j * params_.sigma_j);
    return expected_log_return;
}

double MertonJump_Pricer::price_with_k_jumps(const OptionData& option, const MarketData& market, int k) const {
    // For k jumps, use adjusted volatility and drift
    // σ_k = sqrt(σ^2 + k * σ_j^2 / T)
    // r_k = r - λ(E[J] - 1) + k * μ_j / T
    
    MarketData adjusted_market = market;
    
    // Adjusted volatility for k jumps
    double sigma_squared = market.current_vol * market.current_vol;
    double T = market.time_to_expiry;
    if (T > 0) {
        sigma_squared += (k * params_.sigma_j * params_.sigma_j) / T;
    }
    adjusted_market.current_vol = std::sqrt(std::max(sigma_squared, 0.0));
    
    // Adjusted drift
    // r_effective = r - λ * E[jump] + (ln(1 + expected_jump_contribution) / T)
    double jump_contribution = params_.lambda * params_.mu_j;
    if (T > 0) {
        adjusted_market.risk_free_rate = market.risk_free_rate - jump_contribution;
    }
    
    // Use BSM for the k-jump scenario
    BSM_Pricer bsm;
    return bsm.price(option, adjusted_market);
}

double MertonJump_Pricer::price(const OptionData& option, const MarketData& market) const {
    if (market.time_to_expiry <= 0) {
        return option.intrinsic_value(market.spot_price);
    }
    
    double price_sum = 0.0;
    double cumulative_prob = 0.0;
    
    // Sum over different jump regimes (Merton's series expansion)
    for (int k = 0; k < MAX_JUMPS; ++k) {
        double prob_k = poisson_probability(k);
        
        if (prob_k < JUMP_CUTOFF && k > 5) {
            // Convergence reached, no need to continue
            break;
        }
        
        double price_k = price_with_k_jumps(option, market, k);
        price_sum += prob_k * price_k;
        cumulative_prob += prob_k;
    }
    
    return price_sum;
}

Greeks MertonJump_Pricer::calculate_greeks(const OptionData& option, const MarketData& market) const {
    Greeks greeks;
    
    // Use numerical differentiation for Greeks
    double base_price = price(option, market);
    double h = market.spot_price * 0.01;  // 1% bump
    double h_vol = market.current_vol * 0.01;
    double h_time = 1.0 / 252.0;  // 1 day in fractions of year
    
    // Delta: numerical differentiation w.r.t. spot price
    if (h > 0) {
        MarketData market_up = market;
        market_up.spot_price += h;
        double price_up = price(option, market_up);
        
        MarketData market_down = market;
        market_down.spot_price -= h;
        double price_down = price(option, market_down);
        
        greeks.delta = (price_up - price_down) / (2.0 * h);
    }
    
    // Gamma: second derivative w.r.t. spot
    if (h > 0) {
        MarketData market_up = market;
        market_up.spot_price += h;
        MarketData market_down = market;
        market_down.spot_price -= h;
        
        double price_up = price(option, market_up);
        double price_down = price(option, market_down);
        
        greeks.gamma = (price_up - 2.0 * base_price + price_down) / (h * h);
    }
    
    // Vega: derivative w.r.t. volatility
    if (h_vol > 0) {
        MarketData market_vol_up = market;
        market_vol_up.current_vol += h_vol;
        double price_vol_up = price(option, market_vol_up);
        
        greeks.vega = (price_vol_up - base_price) / h_vol / 100.0;  // per 1%
    }
    
    // Theta: derivative w.r.t. time (negative direction - decay)
    if (h_time > 0) {
        MarketData market_time_down = market;
        market_time_down.time_to_expiry -= h_time;
        if (market_time_down.time_to_expiry > 0) {
            double price_time_down = price(option, market_time_down);
            greeks.theta = -(price_time_down - base_price) / h_time / 365.0;  // per day
        }
    }
    
    // Rho: derivative w.r.t. interest rate
    double h_rate = market.risk_free_rate * 0.01;
    if (h_rate <= 0) h_rate = 0.01;  // Use absolute bump if rate is near zero
    {
        MarketData market_rate_up = market;
        market_rate_up.risk_free_rate += h_rate;
        double price_rate_up = price(option, market_rate_up);
        
        greeks.rho = (price_rate_up - base_price) / h_rate / 100.0;  // per 1%
    }
    
    return greeks;
}

} // namespace hedgebot
