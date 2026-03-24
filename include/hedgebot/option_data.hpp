#pragma once

#include "common.hpp"
#include <string>
#include <ctime>

namespace hedgebot {

// Option contract specification
class OptionData {
public:
    // Constructor
    OptionData(OptionType type, double strike, double expiry, const std::string& symbol = "OPTION")
        : type_(type), strike_(strike), time_to_expiry_(expiry), symbol_(symbol) {}
    
    // Getters
    OptionType type() const { return type_; }
    double strike() const { return strike_; }
    double expiry() const { return time_to_expiry_; }
    const std::string& symbol() const { return symbol_; }
    
    // Check if option is in-the-money
    bool is_itm(double spot_price) const {
        if (type_ == OptionType::CALL) {
            return spot_price > strike_;
        } else {
            return spot_price < strike_;
        }
    }
    
    // Intrinsic value
    double intrinsic_value(double spot_price) const {
        if (type_ == OptionType::CALL) {
            return std::max(0.0, spot_price - strike_);
        } else {
            return std::max(0.0, strike_ - spot_price);
        }
    }
    
    // Payoff at expiry
    double payoff(double spot_price_at_expiry) const {
        return intrinsic_value(spot_price_at_expiry);
    }
    
private:
    OptionType type_;
    double strike_;
    double time_to_expiry_;
    std::string symbol_;
};

} // namespace hedgebot
