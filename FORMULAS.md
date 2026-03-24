# Implementation References & Formulas

## Black-Scholes-Merton Model

### Stochastic Differential Equation
$$dS_t = \mu S_t dt + \sigma S_t dW_t$$

where:
- $S_t$ = asset price at time $t$
- $\mu$ = expected return (not used in pricing)
- $\sigma$ = volatility (constant in BSM)
- $W_t$ = Wiener process (Brownian motion)

### Option Pricing Formula

**European Call:**
$$C = S_0 e^{-dT} N(d_1) - K e^{-rT} N(d_2)$$

**European Put:**
$$P = K e^{-rT} N(-d_2) - S_0 e^{-dT} N(-d_1)$$

where:
$$d_1 = \frac{\ln(S_0/K) + (r - d + \sigma^2/2)T}{\sigma \sqrt{T}}$$
$$d_2 = d_1 - \sigma \sqrt{T}$$

- $S_0$ = current spot price
- $K$ = strike price
- $T$ = time to expiry (years)
- $r$ = risk-free rate
- $d$ = dividend yield
- $\sigma$ = volatility
- $N(x)$ = cumulative standard normal CDF

### Greeks Formulas

| Greek | Symbol | Formula | Interpretation |
|-------|--------|---------|-----------------|
| Delta | $\Delta$ | $\Delta_C = e^{-dT} N(d_1)$ | Price sensitivity to spot price |
| Gamma | $\Gamma$ | $\Gamma = \frac{n(d_1) e^{-dT}}{S \sigma \sqrt{T}}$ | Delta sensitivity (2nd derivative) |
| Vega | $\nu$ | $\nu = S e^{-dT} n(d_1) \sqrt{T}$ | Price sensitivity to volatility (per 1%) |
| Theta | $\Theta$ | $\Theta = -\frac{Sn(d_1)\sigma e^{-dT}}{2\sqrt{T}} - dS e^{-dT}N(d_1) + rK e^{-rT}N(d_2)$ | Time decay per day |
| Rho | $\rho$ | $\rho_C = KT e^{-rT}N(d_2)$ | Price sensitivity to rates (per 1%) |

where:
- $n(x) = \frac{1}{\sqrt{2\pi}} e^{-x^2/2}$ = standard normal PDF
- $N(x)$ = cumulative standard normal CDF

### Put Greeks
For puts, use:
- $\Delta_P = \Delta_C - e^{-dT}$
- $\Gamma_P = \Gamma_C$ (same for both)
- $\nu_P = \nu_C$ (same for both)  
- $\Theta_P = \Theta_C + dS e^{-dT} - rK e^{-rT}$
- $\rho_P = -KT e^{-rT} N(-d_2)$

### Put-Call Parity
$$C - P = S e^{-dT} - K e^{-rT}$$

This is used to validate pricing implementations.

## Merton Jump-Diffusion Model

### Stochastic Differential Equation
$$dS_t = \mu S_t dt + \sigma S_t dW_t + S_{t-} dJ_t$$

where:
- $J_t$ = compound Poisson process with $N(t)$ jumps
- Jump sizes: $ln(1 + Y_i) \sim N(m, \sigma_j^2)$
- Each jump multiplies price by $(1 + Y_i)$

### Parameters to Calibrate
- $\lambda$ = jump intensity (jumps per year) ∈ (0, 1)
- $\mu_j$ = mean log-jump size ∈ ℝ (typically negative)
- $\sigma_j$ = jump volatility ∈ (0, 2)

### Pricing via Series Expansion

The option price under Merton model is a weighted sum:
$$C_{Merton} = \sum_{k=0}^{\infty} P(N_T = k) \cdot C_{BSM}(\sigma_k, r_k)$$

where:
$$P(N_T = k) = \frac{(\lambda T)^k e^{-\lambda T}}{k!}$$ (Poisson probability)

For each state with $k$ jumps:
$$\sigma_k^2 = \sigma^2 + \frac{k \sigma_j^2}{T}$$
$$r_k = r - \lambda \mu_j + \frac{k \mu_j}{T}$$

### Implementation Notes
- Sum terminates when $P(N_T = k) < 10^{-10}$ (usually ~10-15 terms)
- Use BSM pricer recursively for each weighted component
- Greeks via numerical differentiation with ∆S = 1% of spot

## Numerical Implementation

### Normal CDF Approximation
Used Abramowitz and Stegun approximation for better accuracy:
$$N(x) \approx 1 - n(x) \cdot (a_1 t + a_2 t^2 + a_3 t^3 + a_4 t^4 + a_5 t^5)$$

where $t = \frac{1}{1 + px}$ and coefficients are:
- $a_1 = 0.254829592$
- $a_2 = -0.284496736$  
- $a_3 = 1.421413741$
- $a_4 = -1.453152027$
- $a_5 = 1.061405429$
- $p = 0.3275911$

This gives ~7 decimal places of accuracy.

### Greeks Numerical Differentiation
For numerical Greeks (Merton):
$$\Delta \approx \frac{C(S + h) - C(S - h)}{2h}$$ (central difference)
$$\Gamma \approx \frac{C(S + h) - 2C(S) + C(S - h)}{h^2}$$
$$\nu \approx \frac{C(\sigma + h) - C(\sigma)}{h}$$

where typically $h = 0.01 \times S$ for spot, $0.01 \times \sigma$ for vol.

## Calibration Objectives

### BSM Calibration
Fit single parameter $\sigma$ to minimize:
$$\min_\sigma \sum_i (C_{model,i}(\sigma) - C_{market,i})^2$$

Subject to: $\sigma \in (0, 1]$

### Merton Calibration
Fit three parameters $(\lambda, \mu_j, \sigma_j)$ to minimize:
$$\min_{\lambda, \mu_j, \sigma_j} \sum_i (C_{model,i}(\lambda, \mu_j, \sigma_j) - C_{market,i})^2$$

Subject to:
- $\lambda \in (0, 1)$
- $\mu_j \in (-0.5, 0.5)$  
- $\sigma_j \in (0, 2)$

Optimization strategy:
1. Global phase: PSO or genetic algorithm (multiple starting points)
2. Local phase: Levenberg-Marquardt refinement
3. Validate: Check convergence and gradient norm

## Dupire Local Volatility

### Forward Value Definition
$$F(K, T) = \frac{e^{rT} \partial_K C(K,T)}{1 + K \partial_KK C(K,T)}$$

### Local Volatility Formula
$$\sigma_{Dupire}(K, T) = \sqrt{\frac{(r - d)K \partial_K C + \partial_T C + dC}{\frac{1}{2}K^2 \partial_{KK} C}}$$

### Derivation Approach
1. Calibrate to entire option surface (prices for all K, T)
2. Fit smooth surface (splines or polynomial)
3. Compute derivatives numerically
4. Apply Dupire formula at each (K, T) point
5. Regularize to avoid noise in 2nd derivatives

## Heston Stochastic Volatility

### SDEs
$$dS_t = r S_t dt + \sqrt{v_t} S_t dW_S$$
$$dv_t = \kappa(\theta - v_t) dt + \xi \sqrt{v_t} dW_v$$
$$dW_S dW_v = \rho dt$$

### 5 Parameters to Calibrate
- $\kappa$ ∈ (0, 10) = mean reversion speed
- $\theta$ ∈ (0, 1) = long-term variance
- $\xi$ ∈ (0, 5) = volatility of variance
- $\rho$ ∈ [-1, 1] = correlation S-vol
- $v_0$ ∈ (0, 1) = current variance

### Pricing via Characteristic Function
$$C = e^{-rT}[F N(d_1) - K N(d_2)]$$

where $d_1, d_2$ are calculated from Heston's characteristic function via:
- COS method (Fang & Oosterlee 2008)
- FFT methods (Carr & Madan 1999)
- Finite difference PDE solvers

## Software Patterns

### Pricer Pattern
```cpp
class OptionPricer {
    virtual double price(...) = 0;
    virtual Greeks calculate_greeks(...) = 0;
};
```

### Calibrator Pattern  
```cpp
// Objective function wrapper
auto objective = [&](const Eigen::VectorXd& params) {
    return compute_total_error(params, market_prices);
};

// Optimize
optimizer.minimize(objective, initial_guess, bounds);
```

### Hedger Pattern
```cpp
// Calculate target hedge position
Greeks current_greeks = calculate_portfolio_greeks();
double delta_exposure = current_greeks.delta;
double target_hedge = -delta_exposure / underlying_delta;

// Execute
execute_trade(instrument, target_hedge - current_position);
```

## Testing Strategy

### Unit Test Requirements
- **Pricer**: Validate put-call parity, boundary conditions
- **Greeks**: Compare with numerical differentiation
- **Calibrator**: Recover known parameters from synthetic prices
- **Hedger**: Verify delta stays within tolerance after rehedge
- **Risk Manager**: Check VaR/ES calculations vs reference

### Integration Tests
- End-to-end pricing pipeline
- Calibration → Pricing
- Hedging decision → Execution → P&L tracking

### Backtesting Requirements
- 252+ trading days of data
- Multiple market regimes (trending, ranging, volatile)
- Metrics: Sharpe > 1.0, max DD < 15%, hit rate > 55%

## References

[1] Black, F., & Scholes, M. (1973). The pricing of options and corporate liabilities. Journal of Political Economy, 81(3), 637–654.

[2] Merton, R. C. (1976). Option pricing when underlying stock returns are discontinuous. Journal of Financial Economics, 3(1/2), 125–144.

[3] Heston, S. L. (1993). A closed-form solution for options with stochastic volatility with applications to bond and currency options. Review of Financial Studies, 6(2), 327–343.

[4] Dupire, B. (1994). Pricing with a smile. Risk Magazine, 7(1), 18-20.

[5] Fang, F., & Oosterlee, C. W. (2008). A novel pricing method for European options based on Fourier-cosine series expansions. SIAM Journal on Scientific Computing, 31(2), 826-848.
