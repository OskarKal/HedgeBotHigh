# Development Guide - HedgeBot

## Architecture Overview

### Class Hierarchy

```
OptionPricer (abstract base)
├── BSM_Pricer
├── MertonJump_Pricer  
├── LocalVol_Pricer (planned)
└── Heston_Pricer (planned)
```

### Module Dependencies

```
OptionData + MarketData
    ↓
OptionPricer (base)
    ↓
Concrete Pricers (BSM, Merton, etc.)
    ↓
GreeksCalculator
    ↓
ModelCalibrator
    ↓
HedgingEngine
    ↓
BrokerSimulator + RiskManager
    ↓
Backtester + MonteCarloSimulator
```

## Implementation Roadmap

### Phase 1: Foundation ✅ COMPLETE
- [x] Project structure and CMake
- [x] Common types and utilities
- [x] Option and market data structures
- [x] Abstract pricer interface
- [x] BSM pricer with Greeks
- [x] Merton jump-diffusion pricer
- [x] Basic unit tests

### Phase 2: Model Calibration (2-3 weeks)

**Tasks:**
1. Create `ModelCalibrator` interface
   ```cpp
   class ModelCalibrator {
       virtual VectorXd calibrate(const std::vector<OptionPrice>& market_prices) = 0;
   };
   ```

2. Implement BSM calibrator
   - Objective: minimize ||C_model(σ) - C_market||²
   - Use Levenberg-Marquardt optimization
   - Handle bid-ask spread

3. Implement Merton calibrator
   - Multi-parameter optimization (λ, μ_j, σ_j)
   - Use global algorithm (PSO/genetic) + local (L-M)
   - Constraints: λ ∈ (0, 1), σ_j > 0

4. Add Dupire local vol pricer
   - Compute forward smile from volatility surface
   - Apply Dupire's formula: σ²(K,T) = (∂_T C + (r-d)K∂_K C + dC) / (½K² ∂²_K C)

5. Add Heston pricer
   - Characteristic function approach
   - COS method or FFT for efficiency
   - 5 parameters: (κ, θ, ξ, ρ, v0)

**Testing:**
- Unit tests for each calibrator on synthetic data
- Verify calibrated parameters recover known prices
- Benchmark optimization speed (< 1s for BSM, < 10s for Merton)

### Phase 3: Hedging Engine (2-3 weeks)

**Create `HedgingEngine` class:**
```cpp
class HedgingEngine {
public:
    struct RehedgeDecision {
        bool should_rehedge;
        double delta_target;
        double gamma_target;
        double vega_target;
    };
    
    RehedgeDecision check_rehedge(
        const Portfolio& current,
        const MarketData& market,
        const GreeksCalculator& calc
    );
    
    Trade suggest_hedge(const RehedgeDecision& decision);
};
```

**Implementation:**
1. Portfolio tracking class
   - Long/short positions in options and underlying
   - Aggregate Greeks calculation

2. Rehedge decision logic
   - Time-based: rebalance every N hours
   - Threshold-based: rehedge if |Δ_new - Δ_old| > threshold
   - Greeks-based: maintain gamma/vega within bounds

3. Hedge position sizing
   ```
   delta_position = -delta_portfolio / delta_underlying
   gamma_offset = long_gamma_option(strike, vol, T_short)
   vega_position = -vega_portfolio / vega_unit
   ```

4. Futures beta hedging
   - Account for index/stock correlation and beta
   - ES futures for SPX, MES for smaller baskets

5. Protective strategies
   - Collar: long put (5% OTM) + short call (5% OTM)
   - Spread calendar: optimize term structure

**Testing:**
- Verify delta remains neutral after spot move
- Test gamma decay and dynamic rehedging
- Backtesting on 1-month period with daily rehedges

### Phase 4: Risk Management (2 weeks)

**Create `RiskManager` class:**
```cpp
class RiskManager {
    struct RiskMetrics {
        double var_95;
        double es_95;
        double max_drawdown;
        double current_leverage;
    };
    
    RiskMetrics calculate_metrics(const Portfolio& p, const MarketData& m);
    void apply_limits(Portfolio& p, const RiskMetrics& metrics);
    bool check_circuit_breaker(double cumulative_loss);
};
```

**Implementation:**
1. VaR calculation methods
   - Historical: empirical quantile of past returns
   - Parametric: assume normal distribution
   - Monte Carlo: generate paths under current market

2. Position limits
   - Max delta exposure: 0.20
   - Max gamma: 0.05
   - Max leverage: 3.0x
   - Margin requirement tracking

3. Stop-loss and circuit breaker
   - Individual trade stop-loss: -5%
   - Portfolio stop-loss: -10% cumulative
   - Circuit breaker: halt trading on >15% drawdown

4. Real-time monitoring
   - Update metrics every trade execution
   - Alert on threshold violations
   - Log all risk events

**Testing:**
- Verify VaR estimates against historical data
- Test position limit enforcement
- Simulate flash crash scenario

### Phase 5: Backtester (2-3 weeks)

**Create `Backtester` class:**
```cpp
class Backtester {
    struct BacktestResults {
        double total_pnl;
        double sharpe_ratio;
        double max_drawdown;
        double hit_rate;
        std::vector<double> daily_pnl;
    };
    
    BacktestResults run_backtest(
        const std::string& data_file,
        const BacktestConfig& config,
        ModelCalibrator& calibrator,
        HedgingEngine& engine
    );
};
```

**Implementation:**
1. Walk-forward analysis
   - Train period: 30 days → Calibrate models
   - Test period: 1 day → Simulate trading
   - Roll window forward, repeat

2. Daily simulation
   - Mark-to-market all positions using current pricer
   - Calculate Greeks
   - Decide rehedge based on engine
   - Execute and record P&L

3. Performance metrics
   ```
   Sharpe = (μ - r_f) / σ
   Sortino = (μ - r_f) / σ_down
   Calmar = |return| / max_drawdown
   Hit_rate = % winning days
   Recovery_factor = total_profit / max_drawdown
   ```

4. Attribution analysis
   ```
   P&L = theta_pnl + delta_pnl + gamma_pnl + vega_pnl + rho_pnl + vomma_pnl
   ```

**Testing:**
- Run on SPY 2024 calendar year data
- Compare Sharpe ratio across hedging frequencies
- Validate P&L attribution vs. market moves

### Phase 6: Monte Carlo Simulator (1-2 weeks)

**Create `MonteCarloSimulator` class:**
```cpp
class MonteCarloSimulator {
    std::vector<Path> generate_paths(
        const MarketData& market,
        const OptionPricer& pricer,
        int num_paths,
        int num_steps,
        double dt
    );
    
    std::vector<Greeks> estimate_greeks_from_paths(
        const Path& path,
        const OptionData& option
    );
};
```

**Implementation:**
1. Path generation for each model
   - **BSM**: $dS = rS dt + \sigma S dW$
   - **Merton**: Add Poisson jump process
   - **Heston**: Coupled SDEs with correlation

2. Variance reduction techniques
   - Antithetic variates: pair paths with opposites
   - Control variates: use BSM as control
   - Quasi-MC: low-discrepancy sequences (Sobol)

3. Scenario analysis
   - Baseline: historical volatility
   - Vol spike: 50% shock
   - Crisis: 100% vol + negative skew

4. Greek estimation
   - Local regression method
   - Perturbation method
   - Regression on paths

**Testing:**
- Verify path statistics match model parameters
- Compare Monte Carlo Greeks to analytic (BSM)
- Convergence analysis on number of paths

### Phase 7: Additional Features (3+ weeks)

1. **Data Management**
   - CSV loader for market data
   - In-memory cache or database
   - Data validation and interpolation

2. **Optimization Extensions**
   - Differential evolution optimizer
   - Genetic algorithm for global search
   - Parallel calibration

3. **Advanced Pricing Models**
   - SABR model for FX options
   - Local stochastic volatility (Dupire extended)
   - Variance gamma model

4. **Monitoring & Instrumentation**
   - REST API for queries
   - Real-time dashboard
   - Performance profiling
   - Memory leak detection

5. **CI/CD & Deployment**
   - GitLab CI pipeline
   - Automated testing
   - Docker container
   - Kubernetes deployment

## Code Standards

### Naming Conventions
- Classes: `PascalCase` (e.g., `BSM_Pricer`)
- Functions: `snake_case` (e.g., `calculate_greeks`)
- Constants: `UPPER_SNAKE_CASE` (e.g., `EPSILON`)
- Private members: `member_` suffix

### Documentation
- All public functions must have Doxygen comments
- Complex algorithms get inline mathematical references
- Classes documented with usage examples

### Testing
- >90% code coverage target
- Unit tests for all public functions
- Integration tests for module interactions
- Benchmark tests for performance-critical code

### Performance
- Greeks calculation: < 100 μs
- Option pricing: < 10 μs (BSM), < 100 μs (Merton)
- Calibration: < 5s for BSM, < 30s for Merton
- Backtesting: ~1000 days/second

## Build and Deploy

### Local Development
```bash
cmake -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON ..
make -j$(nproc)
make test
make coverage  # Optional: requires gcov
```

### Release Build
```bash
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_LTO=ON ..
make -j$(nproc) install
```

### Docker Deployment
```bash
docker build -t hedgebot:latest .
docker run -v /data:/data hedgebot:latest --config /data/config.yaml
```

## Debugging Tips

### Print Debugging
```cpp
#define HEDGEBOT_DEBUG 1  // Enable detailed logging
#include "hedgebot/logger.hpp"
LOG_DEBUG("Value: {}", price);
```

### Memory Profiling
```bash
valgrind --leak-check=full ./bin/hedge_bot
```

### Performance Profiling
```bash
perf record -g ./bin/hedge_bot
perf report
```

### Correctness Verification
- Print intermediate calculations
- Compare with QuantLib reference implementation
- Validate Greeks with numerical differentiation
- Check boundary conditions (S→0, T→0, σ→0)

## References for Developers

- [Jaan Kiusalaas - Numerical Methods in Engineering with C++](https://www.cambridge.org/core/books/)
- [Press et al. - Numerical Recipes](http://numerical.recipes/)
- [QuantLib documentation](https://www.quantlib.org/)
- [Boost libraries](https://www.boost.org/)

## Questions?

Refer to the specifications in `deep-research-report.md` for detailed requirements.
