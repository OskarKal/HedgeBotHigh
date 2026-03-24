# Quick Start Guide

## Build & Run

```bash
cd /home/oskark/inf/HedgeBot
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make -j$(nproc)

# Run demo
./bin/hedge_bot

# Run tests  
./run_tests
```

## File Structure

```
/home/oskark/inf/HedgeBot/
├── src/
│   ├── main.cpp                 # Demo application
│   ├── bsm_pricer.cpp          # Black-Scholes implementation
│   ├── merton_pricer.cpp       # Merton jump-diffusion
│   └── [other stubs]
├── include/hedgebot/
│   ├── common.hpp              # Math utilities, types
│   ├── option_data.hpp         # OptionData class
│   ├── pricer_base.hpp         # OptionPricer abstract class
│   ├── bsm_pricer.hpp          # BSM_Pricer class
│   ├── merton_pricer.hpp       # MertonJump_Pricer class
│   ├── backtester.hpp          # Backtester + batch backtest APIs
│   ├── monte_carlo_simulator.hpp
│   ├── broker_simulator.hpp
│   └── risk_manager.hpp
├── tests/
│   └── unit_tests.cpp          # Unit tests
├── config/
│   └── bot_config.yaml         # Configuration
├── data/
│   ├── <SYMBOL>/spot_prices.csv
│   ├── <SYMBOL>/option_surface.csv
│   └── RATES/rates.csv
└── CMakeLists.txt              # Build configuration
```

## Multi-Instrument Backtest Layout

The engine now expects one folder per instrument under data, plus a rates folder:

```text
data/
	AAPL/
		spot_prices.csv
		option_surface.csv
	NVDA/
		spot_prices.csv
		option_surface.csv
	...
	RATES/
		rates.csv
```

## Core Classes

### OptionData
```cpp
OptionData call(OptionType::CALL, 100.0, 0.25);  // Call strike 100, T=3mo
OptionData put(OptionType::PUT, 100.0, 0.25);    // Put strike 100
```

### MarketData
```cpp
MarketData market;
market.spot_price = 100.0;
market.risk_free_rate = 0.05;
market.current_vol = 0.20;
market.time_to_expiry = 0.25;
```

### Pricing
```cpp
BSM_Pricer bsm;
double price = bsm.price(call, market);
Greeks greeks = bsm.calculate_greeks(call, market);
```

## Key Formulas

### Black-Scholes
- Call: `C = S·N(d1)·e^(-dT) - K·e^(-rT)·N(d2)`
- Delta: `Δ = e^(-dT)·N(d1)`
- Gamma: `Γ = n(d1)/(S·σ·√T)·e^(-dT)`

### Merton Jump-Diffusion  
- Price: `C = Σ P(k jumps)·C_BSM(adjusted σ, r)`
- Adjustment: `σ_k² = σ² + k·σ_j²/T`

## Configuration (YAML)

Edit `config/bot_config.yaml` for:
- Model parameters (BSM rate, Merton λ/μ_j/σ_j)
- Hedging rules (rebalance frequency, delta threshold)
- Risk limits (max delta, gamma, vega)
- Execution costs (commission, slippage)

## Performance

- **BSM Price**: ~1 microsecond
- **Merton Price**: ~100 microseconds  
- **Greeks Calculation**: ~10 microseconds (BSM), ~50 microseconds (Merton)
- **Compilation**: ~10 seconds

## Next Steps

1. **CI/CD** - run build+tests in GitHub/GitLab pipelines
2. **Docker** - build runtime image and run the demo/backtests
3. **Model Extensions** - add local-vol and Heston pricers
4. **Reporting** - export per-instrument backtest summaries

See `DEVELOPMENT.md` for detailed roadmap.

## Troubleshooting

**Build fails with "Eigen not found"**
- This is OK - project works without Eigen (for now)
- Boost is optional too

**Tests fail**
- Run `./build/run_tests` to check
- All 20 tests should pass

**Price seems wrong**
- Verify input parameters (spot, strike, time, vol, rate)
- Check Greeks (delta should be ~0.5 for ATM call)
- Compare with BSM formula by hand

## References

- Original spec: `deep-research-report.md`
- Code docs: `README.md`, `DEVELOPMENT.md`  
- Configuration: `config/bot_config.yaml`
- Tests: `tests/unit_tests.cpp`
