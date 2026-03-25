#include "hedgebot/live_trading.hpp"

#include <iostream>
#include <algorithm>

namespace {

std::string env_or(const std::unordered_map<std::string, std::string>& env, const std::string& key, const std::string& fallback) {
    const auto it = env.find(key);
    return it == env.end() ? fallback : it->second;
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

} // namespace

using namespace hedgebot;

int main() {
    LiveBotConfig cfg;

    const auto env = DotEnvLoader::load(".env");
    cfg.mode = env_or(env, "HEDGEBOT_EXECUTION_MODE", cfg.mode);
    cfg.symbol = env_or(env, "HEDGEBOT_SYMBOL", cfg.symbol);
    cfg.data_root = env_or(env, "HEDGEBOT_DATA_ROOT", cfg.data_root);
    cfg.strategy_file = env_or(env, "HEDGEBOT_STRATEGY_FILE", cfg.strategy_file);
    cfg.initial_cash = parse_double_or(env_or(env, "HEDGEBOT_INITIAL_CASH", "100000"), cfg.initial_cash);
    cfg.max_steps = parse_size_or(env_or(env, "HEDGEBOT_MAX_STEPS", "240"), cfg.max_steps);
    cfg.use_best_strategy_file = parse_bool_or(env_or(env, "HEDGEBOT_USE_BEST_STRATEGY", "1"), cfg.use_best_strategy_file);

    cfg.broker_api_key = env_or(env, "BROKER_API_KEY", "");
    cfg.broker_api_secret = env_or(env, "BROKER_API_SECRET", "");
    cfg.data_api_key = env_or(env, "DATA_API_KEY", "");

    std::cout << "Starting live bot with symbol=" << cfg.symbol
              << ", mode=" << cfg.mode
              << ", data_root=" << cfg.data_root
              << "\n";

    if (cfg.mode == "live") {
        std::cout << "Mode 'live' requested, but real broker adapter is not yet integrated. "
                     "Falling back to live-dryrun.\n";
        cfg.mode = "live-dryrun";
    }

    LiveTradingBot bot(cfg);
    const bool ok = bot.run();
    return ok ? 0 : 1;
}
