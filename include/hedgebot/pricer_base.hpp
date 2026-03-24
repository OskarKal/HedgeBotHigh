#pragma once

#include "option_data.hpp"
#include "common.hpp"
#include <string>

namespace hedgebot {

// Abstract base class for option pricing
class OptionPricer {
public:
    virtual ~OptionPricer() = default;
    
    // Core pricing methods
    virtual double price(const OptionData& option, const MarketData& market) const = 0;
    
    // Greeks calculation
    virtual Greeks calculate_greeks(const OptionData& option, const MarketData& market) const = 0;
    
    // Pricer name for logging
    virtual std::string name() const = 0;
};

} // namespace hedgebot
