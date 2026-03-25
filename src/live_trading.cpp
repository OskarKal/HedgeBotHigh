#include "hedgebot/live_trading.hpp"

#include "hedgebot/option_data.hpp"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>

namespace hedgebot {

namespace {

std::string trim_copy(const std::string& s) {
    std::size_t b = 0;
    while (b < s.size() && std::isspace(static_cast<unsigned char>(s[b])) != 0) {
        ++b;
    }
    std::size_t e = s.size();
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1])) != 0) {
        --e;
    }
    return s.substr(b, e - b);
}

std::string strip_quotes(const std::string& s) {
    if (s.size() >= 2) {
        if ((s.front() == '"' && s.back() == '"') || (s.front() == '\'' && s.back() == '\'')) {
            return s.substr(1, s.size() - 2);
        }
    }
    return s;
}

double parse_double_or(const std::string& s, double fallback) {
    try {
        return std::stod(s);
    } catch (...) {
        return fallback;
    }
}

std::size_t parse_size_or(const std::string& s, std::size_t fallback) {
    try {
        return static_cast<std::size_t>(std::stoull(s));
    } catch (...) {
        return fallback;
    }
}

bool parse_bool_or(const std::string& s, bool fallback) {
    std::string v = s;
    std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (v == "1" || v == "true" || v == "yes" || v == "on") {
        return true;
    }
    if (v == "0" || v == "false" || v == "no" || v == "off") {
        return false;
    }
    return fallback;
}

std::string env_or(const std::unordered_map<std::string, std::string>& env, const std::string& key, const std::string& fallback) {
    const auto it = env.find(key);
    return it == env.end() ? fallback : it->second;
}

double avg_implied_vol(const std::vector<OptionQuote>& surface) {
    if (surface.empty()) {
        return 0.2;
    }
    double sum = 0.0;
    int n = 0;
    for (const auto& q : surface) {
        if (std::isfinite(q.implied_vol) && q.implied_vol > 0.0) {
            sum += q.implied_vol;
            ++n;
        }
    }
    if (n == 0) {
        return 0.2;
    }
    return sum / static_cast<double>(n);
}

double latest_rate_before(const std::vector<RatePoint>& rates, std::int64_t ts, double fallback) {
    double r = fallback;
    for (const auto& rp : rates) {
        if (rp.timestamp <= ts) {
            r = rp.rate;
        } else {
            break;
        }
    }
    return r;
}

} // namespace

std::unordered_map<std::string, std::string> DotEnvLoader::load(const std::string& path) {
    std::unordered_map<std::string, std::string> out;
    std::ifstream in(path);
    if (!in) {
        return out;
    }

    std::string line;
    while (std::getline(in, line)) {
        std::string t = trim_copy(line);
        if (t.empty() || t[0] == '#') {
            continue;
        }
        const auto eq = t.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string k = trim_copy(t.substr(0, eq));
        std::string v = trim_copy(t.substr(eq + 1));
        out[k] = strip_quotes(v);
    }
    return out;
}

bool StrategyFileLoader::load_best_strategy(
    const std::string& path,
    BacktestConfig& out_backtest,
    HedgeConfig& out_hedge,
    std::string& out_scenario_name) {
    std::ifstream in(path);
    if (!in) {
        return false;
    }

    std::string line;
    while (std::getline(in, line)) {
        const auto pos = line.find(':');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string key = trim_copy(line.substr(0, pos));
        const std::string raw = trim_copy(line.substr(pos + 1));
        const std::string value = strip_quotes(raw);

        if (key == "scenario_name") {
            out_scenario_name = value;
        } else if (key == "initial_cash") {
            out_backtest.initial_cash = parse_double_or(value, out_backtest.initial_cash);
        } else if (key == "risk_free_rate") {
            out_backtest.risk_free_rate = parse_double_or(value, out_backtest.risk_free_rate);
        } else if (key == "dividend_yield") {
            out_backtest.dividend_yield = parse_double_or(value, out_backtest.dividend_yield);
        } else if (key == "calibration_window_steps") {
            out_backtest.calibration_window_steps = parse_size_or(value, out_backtest.calibration_window_steps);
        } else if (key == "option_roll_steps") {
            out_backtest.option_roll_steps = parse_size_or(value, out_backtest.option_roll_steps);
        } else if (key == "strike_moneyness") {
            out_backtest.strike_moneyness = parse_double_or(value, out_backtest.strike_moneyness);
        } else if (key == "option_type") {
            out_backtest.option_type = (value == "PUT") ? OptionType::PUT : OptionType::CALL;
        } else if (key == "option_contracts") {
            out_backtest.option_contracts = parse_double_or(value, out_backtest.option_contracts);
        } else if (key == "delta_threshold") {
            out_hedge.delta_threshold = parse_double_or(value, out_hedge.delta_threshold);
        } else if (key == "gamma_threshold") {
            out_hedge.gamma_threshold = parse_double_or(value, out_hedge.gamma_threshold);
        } else if (key == "vega_threshold") {
            out_hedge.vega_threshold = parse_double_or(value, out_hedge.vega_threshold);
        } else if (key == "rebalance_interval_seconds") {
            out_hedge.rebalance_interval_seconds = static_cast<std::int64_t>(parse_double_or(value, static_cast<double>(out_hedge.rebalance_interval_seconds)));
        }
    }
    return true;
}

PaperOrderExecutor::PaperOrderExecutor(const ExecutionConfig& exec_cfg, double initial_cash)
    : broker_(exec_cfg, initial_cash) {}

FillReport PaperOrderExecutor::place_order(const OrderRequest& req) {
    return broker_.send_order(req);
}

void PaperOrderExecutor::mark_price(const std::string& symbol, double price) {
    broker_.mark_price(symbol, price);
}

double PaperOrderExecutor::position_quantity(const std::string& symbol) const {
    return broker_.get_position(symbol).quantity;
}

double PaperOrderExecutor::total_equity() const {
    return broker_.get_total_equity();
}

DryRunLiveOrderExecutor::DryRunLiveOrderExecutor(double initial_cash) : cash_(initial_cash) {}

FillReport DryRunLiveOrderExecutor::place_order(const OrderRequest& req) {
    FillReport out;
    out.success = true;
    out.symbol = req.symbol;
    out.filled_quantity = req.quantity;
    out.requested_price = req.limit_price;
    out.fill_price = req.limit_price;
    out.notional = std::abs(req.quantity * req.limit_price);
    out.commission = 0.0;
    out.cash_impact = -req.quantity * req.limit_price;

    qty_[req.symbol] += req.quantity;
    cash_ += out.cash_impact;

    return out;
}

void DryRunLiveOrderExecutor::mark_price(const std::string& symbol, double price) {
    marks_[symbol] = price;
}

double DryRunLiveOrderExecutor::position_quantity(const std::string& symbol) const {
    const auto it = qty_.find(symbol);
    return it == qty_.end() ? 0.0 : it->second;
}

double DryRunLiveOrderExecutor::total_equity() const {
    double eq = cash_;
    for (const auto& kv : qty_) {
        const auto mt = marks_.find(kv.first);
        if (mt != marks_.end()) {
            eq += kv.second * mt->second;
        }
    }
    return eq;
}

ReplayMarketFeed::ReplayMarketFeed(const std::string& data_root, const std::string& symbol, double fallback_rate) {
    MarketDataFetcher fetcher;
    DataNormalizer normalizer;

    const auto bundle = fetcher.load_instrument_bundle(data_root, symbol);
    const auto prices = normalizer.sanitize_prices(bundle.prices);
    const auto surf = normalizer.sanitize_option_surface(bundle.option_surface);
    const auto rates = fetcher.load_rates_from_layout(data_root, "RATES", "rates.csv");

    const double iv = avg_implied_vol(surf);
    ticks_.reserve(prices.size());
    for (const auto& p : prices) {
        ticks_.push_back({p.timestamp, p.close, latest_rate_before(rates, p.timestamp, fallback_rate), iv});
    }
}

const std::vector<ReplayTick>& ReplayMarketFeed::ticks() const {
    return ticks_;
}

LiveTradingBot::LiveTradingBot(const LiveBotConfig& cfg) : cfg_(cfg) {}

bool LiveTradingBot::run() {
    BacktestConfig bt;
    HedgeConfig hedge;
    std::string scenario = "default";

    if (cfg_.use_best_strategy_file) {
        const bool ok = StrategyFileLoader::load_best_strategy(cfg_.strategy_file, bt, hedge, scenario);
        if (!ok) {
            std::cerr << "Could not load strategy file: " << cfg_.strategy_file << "\n";
            return false;
        }
    }

    bt.underlying_symbol = cfg_.symbol;
    if (bt.initial_cash <= 0.0) {
        bt.initial_cash = cfg_.initial_cash;
    }

    std::unique_ptr<IOrderExecutor> executor;
    ExecutionConfig exec_cfg;
    exec_cfg.fixed_commission = 0.25;
    exec_cfg.commission_rate = 0.0005;
    exec_cfg.slippage_bps = 1.0;

    if (cfg_.mode == "live-dryrun") {
        executor = std::make_unique<DryRunLiveOrderExecutor>(bt.initial_cash);
    } else {
        executor = std::make_unique<PaperOrderExecutor>(exec_cfg, bt.initial_cash);
    }

    ReplayMarketFeed feed(cfg_.data_root, cfg_.symbol, bt.risk_free_rate);
    const auto& ticks = feed.ticks();
    if (ticks.size() < 3) {
        std::cerr << "Not enough ticks for live loop\n";
        return false;
    }

    BSM_Pricer bsm;
    HedgingEngine hedger(hedge);

    double option_cash = 0.0;
    double option_qty = bt.option_contracts;
    double strike = ticks.front().spot * bt.strike_moneyness;
    OptionData option(bt.option_type, strike, 30.0 / 365.0, "LIVE_OPT");

    MarketData md0;
    md0.spot_price = ticks.front().spot;
    md0.risk_free_rate = ticks.front().rate;
    md0.dividend_yield = bt.dividend_yield;
    md0.time_to_expiry = option.expiry();
    md0.current_vol = ticks.front().implied_vol;
    option_cash = -option_qty * bsm.price(option, md0);

    std::int64_t last_rehedge_ts = ticks.front().timestamp;
    const std::size_t max_steps = std::min(cfg_.max_steps, ticks.size() - 1);

    std::cout << "Live bot mode: " << cfg_.mode << "\n";
    std::cout << "Strategy: " << scenario << "\n";
    std::cout << "Symbol: " << cfg_.symbol << ", steps: " << max_steps << "\n";

    for (std::size_t i = 1; i <= max_steps; ++i) {
        const auto& t_prev = ticks[i - 1];
        const auto& t_cur = ticks[i];

        const double step_years = std::max(1.0 / 31536000.0, static_cast<double>(t_cur.timestamp - t_prev.timestamp) / 31536000.0);
        double remaining_t = std::max(1e-6, option.expiry() - static_cast<double>(i) * step_years);
        if (bt.option_roll_steps > 0 && (i % bt.option_roll_steps == 0)) {
            strike = t_prev.spot * bt.strike_moneyness;
            option = OptionData(bt.option_type, strike, 30.0 / 365.0, "LIVE_OPT");
            remaining_t = option.expiry();
        }

        MarketData md;
        md.spot_price = t_prev.spot;
        md.risk_free_rate = t_prev.rate;
        md.dividend_yield = bt.dividend_yield;
        md.time_to_expiry = remaining_t;
        md.current_vol = t_prev.implied_vol;

        const Greeks g = bsm.calculate_greeks(option, md);
        PortfolioGreeksSnapshot snap;
        snap.underlying_units = executor->position_quantity(cfg_.symbol);
        snap.option_greeks_per_contract = g;
        snap.option_contracts = option_qty;
        snap.last_rebalance_timestamp = last_rehedge_ts;

        const auto decision = hedger.evaluate_rehedge(snap, t_prev.timestamp);
        if (decision.should_rehedge) {
            const auto plan = hedger.build_delta_gamma_hedge_plan(snap);
            if (std::abs(plan.underlying_trade_units) > 1e-12) {
                FillReport fill = executor->place_order({cfg_.symbol, plan.underlying_trade_units, t_prev.spot, t_prev.timestamp});
                if (fill.success) {
                    last_rehedge_ts = t_prev.timestamp;
                    std::cout << "[EXEC] ts=" << t_prev.timestamp
                              << " qty=" << fill.filled_quantity
                              << " px=" << fill.fill_price
                              << " reason=" << decision.reason << "\n";
                }
            }
        }

        executor->mark_price(cfg_.symbol, t_cur.spot);

        MarketData md_mtm = md;
        md_mtm.spot_price = t_cur.spot;
        md_mtm.time_to_expiry = std::max(1e-6, remaining_t - step_years);
        const double option_mtm = option_qty * bsm.price(option, md_mtm);
        const double total_equity = executor->total_equity() + option_cash + option_mtm;

        if (i % 20 == 0 || i == max_steps) {
            std::cout << "[STATE] step=" << i
                      << " spot=" << t_cur.spot
                      << " equity=" << total_equity
                      << " pos=" << executor->position_quantity(cfg_.symbol)
                      << "\n";
        }
    }

    std::cout << "Live loop completed successfully\n";
    return true;
}

} // namespace hedgebot
