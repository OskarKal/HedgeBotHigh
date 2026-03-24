#include "hedgebot/bsm_pricer.hpp"
#include "hedgebot/common.hpp"
#include <cmath>

namespace hedgebot {

double BSM_Pricer::d1(const OptionData& option, const MarketData& market) const {
    double S = market.spot_price;
    double K = option.strike();
    double T = market.time_to_expiry;
    double r = market.risk_free_rate;
    double d = market.dividend_yield;
    double sigma = market.current_vol;
    
    if (sigma <= 0 || T <= 0) {
        throw std::invalid_argument("Invalid volatility or time to expiry in BSM pricer");
    }
    
    return (std::log(S / K) + (r - d + 0.5 * sigma * sigma) * T) / (sigma * std::sqrt(T));
}

double BSM_Pricer::d2(const OptionData& option, const MarketData& market) const {
    double sigma = market.current_vol;
    double T = market.time_to_expiry;
    return d1(option, market) - sigma * std::sqrt(T);
}

double BSM_Pricer::price(const OptionData& option, const MarketData& market) const {
    double S = market.spot_price;
    double K = option.strike();
    double T = market.time_to_expiry;
    double r = market.risk_free_rate;
    double d = market.dividend_yield;
    
    if (T <= 0) {
        // Option is at expiry, return intrinsic value
        return option.intrinsic_value(S);
    }
    
    double d1_val = d1(option, market);
    double d2_val = d2(option, market);
    
    double Nd1 = math::normal_cdf_accurate(d1_val);
    double Nd2 = math::normal_cdf_accurate(d2_val);
    
    if (option.type() == OptionType::CALL) {
        // Call: C = S*e^(-d*T)*N(d1) - K*e^(-r*T)*N(d2)
        return S * std::exp(-d * T) * Nd1 - K * std::exp(-r * T) * Nd2;
    } else {
        // Put: P = K*e^(-r*T)*N(-d2) - S*e^(-d*T)*N(-d1)
        return K * std::exp(-r * T) * (1.0 - Nd2) - S * std::exp(-d * T) * (1.0 - Nd1);
    }
}

double BSM_Pricer::delta(const OptionData& option, const MarketData& market) const {
    double d1_val = d1(option, market);
    double d = market.dividend_yield;
    double T = market.time_to_expiry;
    double S = market.spot_price;
    
    double Nd1 = math::normal_cdf_accurate(d1_val);
    
    if (option.type() == OptionType::CALL) {
        return std::exp(-d * T) * Nd1;
    } else {
        return -std::exp(-d * T) * (1.0 - Nd1);
    }
}

double BSM_Pricer::gamma(const OptionData& option, const MarketData& market) const {
    double d1_val = d1(option, market);
    double d = market.dividend_yield;
    double T = market.time_to_expiry;
    double S = market.spot_price;
    double sigma = market.current_vol;
    
    if (T <= 0 || sigma <= 0) return 0.0;
    
    double numerator = math::normal_pdf(d1_val) * std::exp(-d * T);
    double denominator = S * sigma * std::sqrt(T);
    
    return numerator / denominator;
}

double BSM_Pricer::vega(const OptionData& option, const MarketData& market) const {
    double d1_val = d1(option, market);
    double d = market.dividend_yield;
    double T = market.time_to_expiry;
    double S = market.spot_price;
    
    if (T <= 0) return 0.0;
    
    // Vega per 1% change in volatility
    return S * std::exp(-d * T) * math::normal_pdf(d1_val) * std::sqrt(T) / 100.0;
}

double BSM_Pricer::theta(const OptionData& option, const MarketData& market) const {
    double S = market.spot_price;
    double K = option.strike();
    double T = market.time_to_expiry;
    double r = market.risk_free_rate;
    double d = market.dividend_yield;
    double sigma = market.current_vol;
    
    if (T <= 0) return 0.0;
    
    double d1_val = d1(option, market);
    double d2_val = d2(option, market);
    double Nd1 = math::normal_cdf_accurate(d1_val);
    double Nd2 = math::normal_cdf_accurate(d2_val);
    double n_d1 = math::normal_pdf(d1_val);
    double n_d2 = math::normal_pdf(d2_val);
    
    // Theta per 1 day
    double theta_val = 0.0;
    
    if (option.type() == OptionType::CALL) {
        theta_val = -S * n_d1 * sigma * std::exp(-d * T) / (2.0 * std::sqrt(T));
        theta_val += d * S * Nd1 * std::exp(-d * T);
        theta_val -= r * K * std::exp(-r * T) * Nd2;
    } else {
        theta_val = -S * n_d1 * sigma * std::exp(-d * T) / (2.0 * std::sqrt(T));
        theta_val -= d * S * (1.0 - Nd1) * std::exp(-d * T);
        theta_val += r * K * std::exp(-r * T) * (1.0 - Nd2);
    }
    
    // Convert from per year to per day
    return theta_val / 365.0;
}

double BSM_Pricer::rho(const OptionData& option, const MarketData& market) const {
    double K = option.strike();
    double T = market.time_to_expiry;
    double r = market.risk_free_rate;
    double d2_val = d2(option, market);
    
    if (T <= 0) return 0.0;
    
    double Nd2 = math::normal_cdf_accurate(d2_val);
    
    if (option.type() == OptionType::CALL) {
        // Rho per 1% change in rate
        return K * T * std::exp(-r * T) * Nd2 / 100.0;
    } else {
        return -K * T * std::exp(-r * T) * (1.0 - Nd2) / 100.0;
    }
}

Greeks BSM_Pricer::calculate_greeks(const OptionData& option, const MarketData& market) const {
    Greeks greeks;
    greeks.delta = delta(option, market);
    greeks.gamma = gamma(option, market);
    greeks.vega = vega(option, market);
    greeks.theta = theta(option, market);
    greeks.rho = rho(option, market);
    return greeks;
}

} // namespace hedgebot
