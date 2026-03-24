# HedgeBot - Automated Options & Futures Hedging Bot

Professional C++ trading bot implementing advanced option pricing models and dynamic hedging strategies.

## Project Overview

HedgeBot is a comprehensive automated hedging system for options and futures trading, featuring:

- **Advanced Pricing Models**:
  - Black-Scholes-Merton (BSM)
  - Merton Jump-Diffusion
  - Local Volatility (Dupire) - planned
  - Heston Stochastic Volatility - planned

- **Dynamic Hedging Strategies**:
  - Delta/Gamma/Vega hedging with automated rebalancing
  - Protective collars and spreads
  - Futures-based beta hedging
  - Threshold-based and time-based rebalancing

- **Risk Management**:
  - VaR/ES calculations (historical, parametric, Monte Carlo)
  - Position limits and margin controls
  - Stop-loss and circuit breaker mechanisms
  - Real-time monitoring and alerts

- **Backtesting & Analysis**:
  - Walk-forward analysis with parameter recalibration
  - Monte Carlo simulations with scenario analysis
  - Comprehensive performance metrics (Sharpe ratio, Sortino, Calmar, hit rate)
  - Tail-risk analysis (ES/CVaR)

## Project Structure

```
HedgeBot/
├── src/                    # Implementation files
│   ├── main.cpp           # Entry point
│   ├── bsm_pricer.cpp     # Black-Scholes pricing
│   ├── merton_pricer.cpp  # Merton jump-diffusion
│   ├── model_calibrator.cpp
│   ├── hedging_engine.cpp
│   ├── risk_manager.cpp
│   ├── backtester.cpp
│   ├── monte_carlo_simulator.cpp
│   └── ...
├── include/hedgebot/       # Headers
│   ├── common.hpp         # Common types and math utilities
│   ├── option_data.hpp    # Option contract definitions
│   ├── pricer_base.hpp    # Abstract pricer interface
│   ├── bsm_pricer.hpp
│   ├── merton_pricer.hpp
│   └── ...
├── tests/                  # Unit and integration tests
├── config/                 # Configuration files (YAML)
├── data/                   # Market data (CSV format)
├── build/                  # Build output directory
├── CMakeLists.txt         # Build configuration
└── README.md              # This file
```

## Building the Project

### Requirements
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)
- CMake 3.15+
- Optional: Eigen3, Boost libraries (auto-detected)

### Build Instructions

```bash
cd /home/oskark/inf/HedgeBot
mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Running the Application

```bash
./bin/hedge_bot
```

### Running Tests

```bash
./run_tests
```

## Core Modules

### 1. Option Pricing (`bsm_pricer.cpp`, `merton_pricer.cpp`)

**Black-Scholes-Merton:**
- Implements the closed-form BSM formula for European options
- Calculates Greeks analytically: delta, gamma, vega, theta, rho
- O(1) computation time
- Assumes constant volatility and lognormal price distribution

**Merton Jump-Diffusion:**
- Adds jump component to model tail risk and vol smile
- Uses series expansion for numerical integration
- Adjusts volatility and drift for different jump scenarios
- Models discrete large moves in asset prices

### 2. Greeks Calculation

All pricers implement `calculate_greeks()` returning:
- **Delta** (Δ): Price sensitivity to spot price
- **Gamma** (Γ): Delta sensitivity to spot price (2nd derivative)
- **Vega** (ν): Price sensitivity to volatility
- **Theta** (Θ): Time decay per day
- **Rho** (ρ): Price sensitivity to interest rates

### 3. Model Calibration (Planned)

`ModelCalibrator` will implement:
- **BSM**: Fit σ to implied volatility surface
- **Merton**: Optimize (λ, μ_j, σ_j) to match market prices
- **Dupire**: Calculate local volatility from option prices
- **Heston**: Multi-parameter optimization with global algorithms

### 4. Hedging Engine (Planned)

`HedgingEngine` will manage:
- Real-time Greeks monitoring
- Rebalancing decisions based on thresholds or time
- Position sizing for delta/gamma/vega hedges
- Execution via `BrokerSimulator`

### 5. Risk Management (Planned)

`RiskManager` will track:
- VaR/ES with configurable methods
- Position limits and margin requirements
- Stop-loss orders and circuit breakers
- Real-time alerts on risk breaches

### 6. Backtester (Planned)

`Backtester` will execute:
- Historical simulations with walk-forward analysis
- Parameter recalibration every N days
- Performance metrics and P&L attribution
- Drawdown and tail-risk analysis

### 7. Monte Carlo Simulator (Planned)

`MonteCarloSimulator` will generate:
- Random price paths under various models
- Scenario-based stress testing
- Greek verification and hedging validation
- Confidence intervals for risk estimates

## Pricing Formula Reference

### Black-Scholes-Merton

For European option:
$$C = S e^{-dT} N(d_1) - K e^{-rT} N(d_2)$$

where:
- $d_1 = \frac{\ln(S/K) + (r - d + \sigma^2/2)T}{\sigma\sqrt{T}}$
- $d_2 = d_1 - \sigma\sqrt{T}$
- $N(x)$ is the cumulative normal distribution

### Greeks (BSM)

- **Delta**: $\Delta_C = e^{-dT} N(d_1)$
- **Gamma**: $\Gamma = \frac{n(d_1)}{S\sigma\sqrt{T}} e^{-dT}$
- **Vega**: $\nu = S e^{-dT} n(d_1) \sqrt{T}$
- **Theta**: $\Theta = -\frac{S n(d_1) \sigma}{2\sqrt{T}} e^{-dT} - dS e^{-dT}N(d_1) + rK e^{-rT}N(d_2)$

### Merton Jump-Diffusion

Price as weighted sum of BSM prices:
$$C_{Merton} = \sum_{k=0}^{\infty} P(N_T=k) \cdot C_{BSM}(\sigma_k, r_k)$$

where:
- $\sigma_k^2 = \sigma^2 + k\sigma_j^2/T$
- $r_k = r - \lambda\mu_j + k\mu_j/T$
- $P(N_T=k) = \frac{(\lambda T)^k e^{-\lambda T}}{k!}$

## Configuration

The bot is configured via `config/bot_config.yaml`:

```yaml
bot_config:
  models:
    BSM: {rate: 0.05}
    Merton: {lambda: 0.1, mu_j: -0.05, sigma_j: 0.15}
  
  hedging:
    rebalance_interval: 3600    # seconds
    delta_threshold: 0.05        # 5%
  
  execution:
    commission: 0.001            # 0.1%
    slippage_model: "spread+0.001"
```

## Current Status

### ✅ Completed
- Project structure and CMake configuration
- Core mathematical utilities and types
- Black-Scholes-Merton pricer with analytics Greeks
- Merton Jump-Diffusion pricer with numerical Greeks
- Unit test framework
- Configuration file template
- Documentation

### 🔄 In Progress
- Greeks calculator module extension

### 📋 To Do
- Model calibrator (LM, Nelder-Mead, PSO optimization)
- Hedging engine (rebalancing logic, instrument selection)
- Broker simulator (order execution, P&L tracking)
- Risk manager (VaR, limits, circuit breaker)
- Backtester (walk-forward, metrics calculation)
- Monte Carlo simulator (path generation, scenarios)
- Additional pricers (Dupire, Heston)
- CI/CD pipeline and Docker containerization
- REST API monitoring interface
- Production hardening

## Usage Examples

### Basic Pricing

```cpp
#include "hedgebot/bsm_pricer.hpp"

MarketData market;
market.spot_price = 100.0;
market.risk_free_rate = 0.05;
market.dividend_yield = 0.02;
market.time_to_expiry = 0.25;  // 3 months
market.current_vol = 0.20;      // 20% volatility

OptionData call(OptionType::CALL, 100.0, 0.25);
BSM_Pricer pricer;

double price = pricer.price(call, market);
Greeks greeks = pricer.calculate_greeks(call, market);

std::cout << "Call Price: " << price << "\n";
std::cout << "Delta: " << greeks.delta << "\n";
std::cout << "Gamma: " << greeks.gamma << "\n";
```

### Comparing Models

```cpp
BSM_Pricer bsm;
MertonJump_Pricer::Parameters merton_params(0.1, -0.05, 0.15);
MertonJump_Pricer merton(merton_params);

double bsm_price = bsm.price(option, market);
double merton_price = merton.price(option, market);

// Jump premium
double jump_premium = merton_price - bsm_price;
```

## Next Steps

1. **Implement Model Calibrator** (~2-3 weeks)
   - Optimize BSM volatility to market prices
   - Fit Merton parameters to vol smile
   - Add Dupire and Heston pricers

2. **Build Hedging Engine** (~2-3 weeks)
   - Real-time Greeks monitoring
   - Rebalancing logic and thresholds
   - Delta/gamma/vega hedging positions

3. **Add Risk Management** (~2 weeks)
   - VaR calculations (historical, parametric)
   - Position limits and margin tracking
   - Circuit breaker automation

4. **Create Backtester** (~2-3 weeks)
   - Walk-forward parameter optimization
   - Historical simulation engine
   - Performance metrics and reporting

5. **Implement Monte Carlo** (~1-2 weeks)
   - Path generation for different models
   - Scenario analysis and stress testing
   - Greek verification

6. **Production Hardening** (~2-3 weeks)
   - Comprehensive error handling
   - Performance optimization
   - CI/CD pipeline and Docker
   - REST API interface
   - Logging and monitoring

## Testing Strategy

- **Unit Tests**: Math utilities, Greeks calculations, pricer validation
- **Integration Tests**: End-to-end pricing and hedging workflows
- **Backtests**: Historical data on SPY, VIX futures, index options
- **Stress Tests**: Market shocks, vol spikes, tail scenarios
- **Performance Tests**: Calibration speed, rebalancing latency

## References

- Black, F., Scholes, M. (1973). "The pricing of options and corporate liabilities"
- Merton, R. C. (1976). "Option pricing when underlying stock returns are discontinuous"
- Heston, S. L. (1993). "A closed-form solution for options with stochastic volatility"
- Dupire, B. (1994). "Pricing with a smile"

## License

MIT License - See LICENSE file for details

## Author

HedgeBot Development Team
