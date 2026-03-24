#pragma once

#include <vector>
#include <cmath>
#include <stdexcept>
#include <limits>
#include <functional>

namespace hedgebot {

// Mathematical constants
constexpr double PI = M_PI;
constexpr double SQRT_2PI = std::sqrt(2.0 * PI);
constexpr double EPSILON = std::numeric_limits<double>::epsilon();

// Common numerical methods
namespace math {

// Standard normal CDF using error function
inline double normal_cdf(double x) {
    return 0.5 * (1.0 + std::erf(x / std::sqrt(2.0)));
}

// Standard normal PDF
inline double normal_pdf(double x) {
    return std::exp(-0.5 * x * x) / SQRT_2PI;
}

// Cumulative standard normal distribution - more accurate Abramowitz and Stegun approximation
inline double normal_cdf_accurate(double x) {
    if (x < -6.0) return 0.0;
    if (x > 6.0) return 1.0;
    
    double a1 = 0.254829592;
    double a2 = -0.284496736;
    double a3 = 1.421413741;
    double a4 = -1.453152027;
    double a5 = 1.061405429;
    double p = 0.3275911;
    
    int sign = (x < 0) ? -1 : 1;
    x = std::abs(x) / std::sqrt(2.0);
    
    double t = 1.0 / (1.0 + p * x);
    double t2 = t * t;
    double t3 = t2 * t;
    double t4 = t2 * t2;
    double t5 = t4 * t;
    
    double y = 1.0 - (((((a5 * t5 + a4 * t4) + a3 * t3) + a2 * t2) + a1 * t) * std::exp(-x * x));
    
    return 0.5 * (1.0 + sign * y);
}

} // namespace math

// Option type enumeration
enum class OptionType {
    CALL = 1,
    PUT = -1
};

// Market conditions snapshot
struct MarketData {
    double spot_price;      // S
    double risk_free_rate;  // r
    double dividend_yield;  // d (for stocks)
    double time_to_expiry;  // T (in years)
    double current_vol;     // current implied volatility estimate
};

// Greeks structure
struct Greeks {
    double delta;    // ∂V/∂S
    double gamma;    // ∂²V/∂S²
    double vega;     // ∂V/∂σ (per 1% change)
    double theta;    // ∂V/∂t (per 1 day)
    double rho;      // ∂V/∂r (per 1% change)
    double vanna;    // ∂²V/∂S∂σ (per 1% vol)
    double volga;    // ∂²V/∂σ²
    
    Greeks() : delta(0), gamma(0), vega(0), theta(0), rho(0), vanna(0), volga(0) {}
};

} // namespace hedgebot
