#pragma once

#include "pricer_base.hpp"

namespace hedgebot {
class BSM_Pricer : public OptionPricer {
public:
    BSM_Pricer() = default;
    virtual ~BSM_Pricer() = default;
    
    // Price using Black-Scholes-Merton formula
    double price(const OptionData& option, const MarketData& market) const override;
    
    // Calculate all Greeks using analytical formulas
    Greeks calculate_greeks(const OptionData& option, const MarketData& market) const override;
    
    // Individual Greek calculations
    double delta(const OptionData& option, const MarketData& market) const;
    double gamma(const OptionData& option, const MarketData& market) const;
    double vega(const OptionData& option, const MarketData& market) const;
    double theta(const OptionData& option, const MarketData& market) const;
    double rho(const OptionData& option, const MarketData& market) const;
    
    std::string name() const override { return "Black-Scholes-Merton"; }
    
private:
    // Helper functions for d1 and d2 parameters in BSM formula
    double d1(const OptionData& option, const MarketData& market) const;
    double d2(const OptionData& option, const MarketData& market) const;
};

} // namespace hedgebot
