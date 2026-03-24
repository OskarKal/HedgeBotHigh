#pragma once

#include "common.hpp"

#include <cstdint>
#include <string>
#include <vector>

namespace hedgebot {

struct PriceBar {
    std::int64_t timestamp;
    double close;
};

struct RatePoint {
    std::int64_t timestamp;
    double rate;
};

struct OptionQuote {
    OptionType type;
    double strike;
    double maturity;
    double bid;
    double ask;
    double implied_vol;

    double mid() const { return 0.5 * (bid + ask); }
};

struct InstrumentDataBundle {
    std::string symbol;
    std::vector<PriceBar> prices;
    std::vector<OptionQuote> option_surface;
};

class MarketDataFetcher {
public:
    std::vector<PriceBar> load_price_data(const std::string& file_path) const;
    std::vector<OptionQuote> load_option_surface(const std::string& file_path) const;
    std::vector<RatePoint> load_rate_data(const std::string& file_path) const;

    std::vector<std::string> discover_instruments(const std::string& data_root) const;
    InstrumentDataBundle load_instrument_bundle(const std::string& data_root, const std::string& symbol) const;
    std::vector<InstrumentDataBundle> load_all_instruments(const std::string& data_root) const;
    std::vector<RatePoint> load_rates_from_layout(
        const std::string& data_root,
        const std::string& rates_folder = "RATES",
        const std::string& rates_file = "rates.csv") const;

private:
    static std::vector<std::string> split_csv_line(const std::string& line);
};

class DataNormalizer {
public:
    std::vector<PriceBar> sanitize_prices(const std::vector<PriceBar>& prices) const;
    std::vector<PriceBar> fill_missing_prices_linear(
        const std::vector<PriceBar>& prices,
        std::int64_t expected_step_seconds) const;

    std::vector<double> zscore_prices(const std::vector<PriceBar>& prices) const;

    std::vector<OptionQuote> sanitize_option_surface(const std::vector<OptionQuote>& surface) const;
    std::vector<OptionQuote> fill_missing_implied_vol_by_strike(
        const std::vector<OptionQuote>& surface,
        OptionType type,
        double maturity) const;
};

} // namespace hedgebot
