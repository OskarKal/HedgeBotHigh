#include "hedgebot/market_data.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace hedgebot {

namespace {

bool is_invalid_number(double value) {
	return std::isnan(value) || std::isinf(value);
}

double parse_double(const std::string& token, const std::string& field_name) {
	try {
		std::size_t idx = 0;
		double value = std::stod(token, &idx);
		if (idx != token.size()) {
			throw std::invalid_argument("extra characters");
		}
		return value;
	} catch (const std::exception&) {
		throw std::runtime_error("Failed to parse field '" + field_name + "' with value '" + token + "'");
	}
}

std::int64_t parse_int64(const std::string& token, const std::string& field_name) {
	try {
		std::size_t idx = 0;
		long long value = std::stoll(token, &idx, 10);
		if (idx != token.size()) {
			throw std::invalid_argument("extra characters");
		}
		return static_cast<std::int64_t>(value);
	} catch (const std::exception&) {
		throw std::runtime_error("Failed to parse field '" + field_name + "' with value '" + token + "'");
	}
}

std::string to_upper_ascii(std::string s) {
	for (char& c : s) {
		c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
	}
	return s;
}

} // namespace

std::vector<std::string> MarketDataFetcher::split_csv_line(const std::string& line) {
	std::vector<std::string> tokens;
	std::stringstream ss(line);
	std::string token;

	while (std::getline(ss, token, ',')) {
		tokens.push_back(token);
	}
	return tokens;
}

std::vector<PriceBar> MarketDataFetcher::load_price_data(const std::string& file_path) const {
	std::ifstream in(file_path);
	if (!in.is_open()) {
		throw std::runtime_error("Cannot open price data file: " + file_path);
	}

	std::vector<PriceBar> result;
	std::string line;
	bool first_line = true;
	while (std::getline(in, line)) {
		if (line.empty()) {
			continue;
		}

		const auto tokens = split_csv_line(line);
		if (tokens.size() < 2) {
			continue;
		}

		if (first_line) {
			first_line = false;
			if (!std::isdigit(static_cast<unsigned char>(tokens[0][0])) && tokens[0][0] != '-') {
				continue;
			}
		}

		PriceBar row;
		row.timestamp = parse_int64(tokens[0], "timestamp");
		row.close = parse_double(tokens[1], "close");
		result.push_back(row);
	}

	std::sort(result.begin(), result.end(), [](const PriceBar& a, const PriceBar& b) {
		return a.timestamp < b.timestamp;
	});
	return result;
}

std::vector<OptionQuote> MarketDataFetcher::load_option_surface(const std::string& file_path) const {
	std::ifstream in(file_path);
	if (!in.is_open()) {
		throw std::runtime_error("Cannot open option surface file: " + file_path);
	}

	std::vector<OptionQuote> result;
	std::string line;
	bool schema_initialized = false;
	std::size_t idx_type = 0;
	std::size_t idx_strike = 1;
	std::size_t idx_maturity = 2;
	std::size_t idx_bid = 3;
	std::size_t idx_ask = 4;
	std::size_t idx_iv = 5;

	while (std::getline(in, line)) {
		if (line.empty()) {
			continue;
		}

		const auto tokens = split_csv_line(line);
		if (tokens.size() < 6) {
			continue;
		}

		if (!schema_initialized) {
			schema_initialized = true;

			// Header-based schema detection.
			bool has_header = false;
			for (std::size_t i = 0; i < tokens.size(); ++i) {
				const std::string h = to_upper_ascii(tokens[i]);
				if (h == "TYPE") {
					idx_type = i;
					has_header = true;
				} else if (h == "STRIKE") {
					idx_strike = i;
					has_header = true;
				} else if (h == "MATURITY") {
					idx_maturity = i;
					has_header = true;
				} else if (h == "BID") {
					idx_bid = i;
					has_header = true;
				} else if (h == "ASK") {
					idx_ask = i;
					has_header = true;
				} else if (h == "IMPLIED_VOL") {
					idx_iv = i;
					has_header = true;
				}
			}

			if (has_header) {
				continue;
			}

			// No header: if first column is numeric timestamp and second is option type,
			// map indices to [type,strike,maturity,bid,ask,iv] starting at column 1.
			if (tokens.size() >= 7) {
				const std::string t1 = to_upper_ascii(tokens[1]);
				if (t1 == "CALL" || t1 == "PUT" || t1 == "C" || t1 == "P") {
					idx_type = 1;
					idx_strike = 2;
					idx_maturity = 3;
					idx_bid = 4;
					idx_ask = 5;
					idx_iv = 6;
				}
			}
		}

		if (tokens.size() <= std::max({idx_type, idx_strike, idx_maturity, idx_bid, idx_ask, idx_iv})) {
			continue;
		}

		OptionQuote row;
		const std::string type_token = to_upper_ascii(tokens[idx_type]);
		row.type = (type_token == "C" || type_token == "CALL") ? OptionType::CALL : OptionType::PUT;
		row.strike = parse_double(tokens[idx_strike], "strike");
		row.maturity = parse_double(tokens[idx_maturity], "maturity");
		row.bid = parse_double(tokens[idx_bid], "bid");
		row.ask = parse_double(tokens[idx_ask], "ask");
		row.implied_vol = parse_double(tokens[idx_iv], "implied_vol");
		result.push_back(row);
	}

	std::sort(result.begin(), result.end(), [](const OptionQuote& a, const OptionQuote& b) {
		if (a.maturity == b.maturity) {
			if (a.type == b.type) {
				return a.strike < b.strike;
			}
			return static_cast<int>(a.type) < static_cast<int>(b.type);
		}
		return a.maturity < b.maturity;
	});
	return result;
}

std::vector<RatePoint> MarketDataFetcher::load_rate_data(const std::string& file_path) const {
	std::ifstream in(file_path);
	if (!in.is_open()) {
		throw std::runtime_error("Cannot open rate data file: " + file_path);
	}

	std::vector<RatePoint> result;
	std::string line;
	bool first_line = true;
	while (std::getline(in, line)) {
		if (line.empty()) {
			continue;
		}

		const auto tokens = split_csv_line(line);
		if (tokens.size() < 2) {
			continue;
		}

		if (first_line) {
			first_line = false;
			if (!std::isdigit(static_cast<unsigned char>(tokens[0][0])) && tokens[0][0] != '-') {
				continue;
			}
		}

		RatePoint row;
		row.timestamp = parse_int64(tokens[0], "timestamp");
		row.rate = parse_double(tokens[1], "rate");
		result.push_back(row);
	}

	std::sort(result.begin(), result.end(), [](const RatePoint& a, const RatePoint& b) {
		return a.timestamp < b.timestamp;
	});
	return result;
}

std::vector<std::string> MarketDataFetcher::discover_instruments(const std::string& data_root) const {
	namespace fs = std::filesystem;
	fs::path root(data_root);
	if (!fs::exists(root) || !fs::is_directory(root)) {
		throw std::runtime_error("discover_instruments: data root is not a directory: " + data_root);
	}

	std::vector<std::string> symbols;
	for (const auto& entry : fs::directory_iterator(root)) {
		if (!entry.is_directory()) {
			continue;
		}

		const std::string name = entry.path().filename().string();
		if (to_upper_ascii(name) == "RATES") {
			continue;
		}

		const fs::path spot = entry.path() / "spot_prices.csv";
		const fs::path surface = entry.path() / "option_surface.csv";
		if (fs::exists(spot) && fs::exists(surface)) {
			symbols.push_back(name);
		}
	}

	std::sort(symbols.begin(), symbols.end());
	return symbols;
}

InstrumentDataBundle MarketDataFetcher::load_instrument_bundle(const std::string& data_root, const std::string& symbol) const {
	namespace fs = std::filesystem;
	const fs::path base = fs::path(data_root) / symbol;

	InstrumentDataBundle bundle;
	bundle.symbol = symbol;
	bundle.prices = load_price_data((base / "spot_prices.csv").string());
	bundle.option_surface = load_option_surface((base / "option_surface.csv").string());
	return bundle;
}

std::vector<InstrumentDataBundle> MarketDataFetcher::load_all_instruments(const std::string& data_root) const {
	std::vector<InstrumentDataBundle> all;
	for (const auto& symbol : discover_instruments(data_root)) {
		all.push_back(load_instrument_bundle(data_root, symbol));
	}
	return all;
}

std::vector<RatePoint> MarketDataFetcher::load_rates_from_layout(
	const std::string& data_root,
	const std::string& rates_folder,
	const std::string& rates_file) const {
	namespace fs = std::filesystem;
	const fs::path path = fs::path(data_root) / rates_folder / rates_file;
	return load_rate_data(path.string());
}

std::vector<PriceBar> DataNormalizer::sanitize_prices(const std::vector<PriceBar>& prices) const {
	std::vector<PriceBar> out;
	out.reserve(prices.size());
	for (const auto& p : prices) {
		if (p.timestamp <= 0) {
			continue;
		}
		if (p.close <= 0.0 || is_invalid_number(p.close)) {
			continue;
		}
		out.push_back(p);
	}

	std::sort(out.begin(), out.end(), [](const PriceBar& a, const PriceBar& b) {
		return a.timestamp < b.timestamp;
	});

	out.erase(std::unique(out.begin(), out.end(), [](const PriceBar& a, const PriceBar& b) {
		return a.timestamp == b.timestamp;
	}), out.end());

	return out;
}

std::vector<PriceBar> DataNormalizer::fill_missing_prices_linear(
	const std::vector<PriceBar>& prices,
	std::int64_t expected_step_seconds) const {
	if (prices.empty() || expected_step_seconds <= 0) {
		return prices;
	}

	std::vector<PriceBar> sorted = prices;
	std::sort(sorted.begin(), sorted.end(), [](const PriceBar& a, const PriceBar& b) {
		return a.timestamp < b.timestamp;
	});

	std::vector<PriceBar> out;
	out.reserve(sorted.size() * 2);

	for (std::size_t i = 0; i < sorted.size() - 1; ++i) {
		const auto& cur = sorted[i];
		const auto& nxt = sorted[i + 1];
		out.push_back(cur);

		const std::int64_t gap = nxt.timestamp - cur.timestamp;
		if (gap <= expected_step_seconds) {
			continue;
		}

		const int missing = static_cast<int>(gap / expected_step_seconds) - 1;
		for (int k = 1; k <= missing; ++k) {
			const double alpha = static_cast<double>(k) / static_cast<double>(missing + 1);
			PriceBar filled;
			filled.timestamp = cur.timestamp + static_cast<std::int64_t>(k) * expected_step_seconds;
			filled.close = cur.close + alpha * (nxt.close - cur.close);
			out.push_back(filled);
		}
	}

	out.push_back(sorted.back());
	return out;
}

std::vector<double> DataNormalizer::zscore_prices(const std::vector<PriceBar>& prices) const {
	std::vector<double> values;
	values.reserve(prices.size());
	for (const auto& p : prices) {
		values.push_back(p.close);
	}

	if (values.empty()) {
		return {};
	}

	const double mean = std::accumulate(values.begin(), values.end(), 0.0) / static_cast<double>(values.size());
	double variance = 0.0;
	for (double v : values) {
		const double d = v - mean;
		variance += d * d;
	}
	variance /= static_cast<double>(values.size());
	const double stddev = std::sqrt(variance);

	std::vector<double> zscores(values.size(), 0.0);
	if (stddev < 1e-12) {
		return zscores;
	}

	for (std::size_t i = 0; i < values.size(); ++i) {
		zscores[i] = (values[i] - mean) / stddev;
	}
	return zscores;
}

std::vector<OptionQuote> DataNormalizer::sanitize_option_surface(const std::vector<OptionQuote>& surface) const {
	std::vector<OptionQuote> out;
	out.reserve(surface.size());
	for (const auto& q : surface) {
		if (q.strike <= 0.0 || q.maturity <= 0.0) {
			continue;
		}
		if (q.bid < 0.0 || q.ask < 0.0 || q.ask < q.bid) {
			continue;
		}
		if (q.implied_vol <= 0.0 || is_invalid_number(q.implied_vol)) {
			continue;
		}
		out.push_back(q);
	}

	std::sort(out.begin(), out.end(), [](const OptionQuote& a, const OptionQuote& b) {
		if (a.type == b.type) {
			if (a.maturity == b.maturity) {
				return a.strike < b.strike;
			}
			return a.maturity < b.maturity;
		}
		return static_cast<int>(a.type) < static_cast<int>(b.type);
	});
	return out;
}

std::vector<OptionQuote> DataNormalizer::fill_missing_implied_vol_by_strike(
	const std::vector<OptionQuote>& surface,
	OptionType type,
	double maturity) const {
	std::vector<OptionQuote> filtered;
	for (const auto& q : surface) {
		if (q.type == type && std::abs(q.maturity - maturity) < 1e-10) {
			filtered.push_back(q);
		}
	}

	if (filtered.size() < 2) {
		return filtered;
	}

	std::sort(filtered.begin(), filtered.end(), [](const OptionQuote& a, const OptionQuote& b) {
		return a.strike < b.strike;
	});

	// Fill only entries with missing IV (<= 0) by linear interpolation over strike.
	for (std::size_t i = 0; i < filtered.size(); ++i) {
		if (filtered[i].implied_vol > 0.0) {
			continue;
		}

		int left = static_cast<int>(i) - 1;
		while (left >= 0 && filtered[static_cast<std::size_t>(left)].implied_vol <= 0.0) {
			--left;
		}

		std::size_t right = i + 1;
		while (right < filtered.size() && filtered[right].implied_vol <= 0.0) {
			++right;
		}

		if (left >= 0 && right < filtered.size()) {
			const auto& l = filtered[static_cast<std::size_t>(left)];
			const auto& r = filtered[right];
			const double weight = (filtered[i].strike - l.strike) / (r.strike - l.strike);
			filtered[i].implied_vol = l.implied_vol + weight * (r.implied_vol - l.implied_vol);
		} else if (left >= 0) {
			filtered[i].implied_vol = filtered[static_cast<std::size_t>(left)].implied_vol;
		} else if (right < filtered.size()) {
			filtered[i].implied_vol = filtered[right].implied_vol;
		}
	}

	return filtered;
}

} // namespace hedgebot
