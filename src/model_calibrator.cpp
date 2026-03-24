#include "hedgebot/model_calibrator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace hedgebot {

namespace {

double clamp_value(double v, double lo, double hi) {
    return std::max(lo, std::min(v, hi));
}

OptionData to_option_data(const OptionQuote& q) {
    return OptionData(q.type, q.strike, q.maturity);
}

double quote_mid(const OptionQuote& q) {
    return 0.5 * (q.bid + q.ask);
}

} // namespace

double ModelCalibrator::bsm_objective(
    const std::vector<OptionQuote>& surface,
    const MarketData& market_base,
    double sigma) {
    if (surface.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    BSM_Pricer pricer;
    double sq_error = 0.0;

    for (const auto& q : surface) {
        MarketData md = market_base;
        md.current_vol = sigma;
        md.time_to_expiry = q.maturity;

        const OptionData option = to_option_data(q);
        const double model_price = pricer.price(option, md);
        const double market_price = quote_mid(q);
        const double diff = model_price - market_price;
        sq_error += diff * diff;
    }

    return sq_error / static_cast<double>(surface.size());
}

double ModelCalibrator::merton_objective(
    const std::vector<OptionQuote>& surface,
    const MarketData& market_base,
    const MertonJump_Pricer::Parameters& params) {
    if (surface.empty()) {
        return std::numeric_limits<double>::infinity();
    }

    MertonJump_Pricer pricer(params);
    double sq_error = 0.0;

    for (const auto& q : surface) {
        MarketData md = market_base;
        md.time_to_expiry = q.maturity;

        const OptionData option = to_option_data(q);
        const double model_price = pricer.price(option, md);
        const double market_price = quote_mid(q);
        const double diff = model_price - market_price;
        sq_error += diff * diff;
    }

    return sq_error / static_cast<double>(surface.size());
}

ModelCalibrator::BSMResult ModelCalibrator::calibrate_bsm_vol(
    const std::vector<OptionQuote>& surface,
    const MarketData& market_base,
    double min_sigma,
    double max_sigma,
    int grid_steps) const {
    if (surface.empty()) {
        throw std::invalid_argument("calibrate_bsm_vol: surface cannot be empty");
    }
    if (min_sigma <= 0.0 || max_sigma <= min_sigma) {
        throw std::invalid_argument("calibrate_bsm_vol: invalid sigma bounds");
    }
    if (grid_steps < 3) {
        throw std::invalid_argument("calibrate_bsm_vol: grid_steps must be >= 3");
    }

    const double step = (max_sigma - min_sigma) / static_cast<double>(grid_steps - 1);
    double best_sigma = min_sigma;
    double best_obj = std::numeric_limits<double>::infinity();

    for (int i = 0; i < grid_steps; ++i) {
        const double sigma = min_sigma + step * static_cast<double>(i);
        const double obj = bsm_objective(surface, market_base, sigma);
        if (obj < best_obj) {
            best_obj = obj;
            best_sigma = sigma;
        }
    }

    // Local refinement with ternary search around the coarse optimum.
    double lo = clamp_value(best_sigma - 2.0 * step, min_sigma, max_sigma);
    double hi = clamp_value(best_sigma + 2.0 * step, min_sigma, max_sigma);
    int local_iters = 0;
    while ((hi - lo) > 1e-6 && local_iters < 80) {
        const double m1 = lo + (hi - lo) / 3.0;
        const double m2 = hi - (hi - lo) / 3.0;
        const double f1 = bsm_objective(surface, market_base, m1);
        const double f2 = bsm_objective(surface, market_base, m2);
        if (f1 <= f2) {
            hi = m2;
        } else {
            lo = m1;
        }
        ++local_iters;
    }

    best_sigma = 0.5 * (lo + hi);
    best_obj = bsm_objective(surface, market_base, best_sigma);

    BSMResult out;
    out.sigma = best_sigma;
    out.summary = {true, best_obj, grid_steps + local_iters};
    return out;
}

ModelCalibrator::MertonResult ModelCalibrator::calibrate_merton_params(
    const std::vector<OptionQuote>& surface,
    const MarketData& market_base,
    double min_lambda,
    double max_lambda,
    double min_mu_j,
    double max_mu_j,
    double min_sigma_j,
    double max_sigma_j,
    int lambda_steps,
    int mu_steps,
    int sigma_j_steps) const {
    if (surface.empty()) {
        throw std::invalid_argument("calibrate_merton_params: surface cannot be empty");
    }
    if (lambda_steps < 2 || mu_steps < 2 || sigma_j_steps < 2) {
        throw std::invalid_argument("calibrate_merton_params: step counts must be >= 2");
    }

    const double d_lambda = (max_lambda - min_lambda) / static_cast<double>(lambda_steps - 1);
    const double d_mu = (max_mu_j - min_mu_j) / static_cast<double>(mu_steps - 1);
    const double d_sigma_j = (max_sigma_j - min_sigma_j) / static_cast<double>(sigma_j_steps - 1);

    MertonJump_Pricer::Parameters best_params(min_lambda, min_mu_j, min_sigma_j);
    double best_obj = std::numeric_limits<double>::infinity();
    int iters = 0;

    for (int i = 0; i < lambda_steps; ++i) {
        const double lambda = min_lambda + d_lambda * static_cast<double>(i);
        for (int j = 0; j < mu_steps; ++j) {
            const double mu_j = min_mu_j + d_mu * static_cast<double>(j);
            for (int k = 0; k < sigma_j_steps; ++k) {
                const double sigma_j = min_sigma_j + d_sigma_j * static_cast<double>(k);
                const MertonJump_Pricer::Parameters params(lambda, mu_j, sigma_j);
                const double obj = merton_objective(surface, market_base, params);
                ++iters;

                if (obj < best_obj) {
                    best_obj = obj;
                    best_params = params;
                }
            }
        }
    }

    MertonResult out;
    out.params = best_params;
    out.summary = {true, best_obj, iters};
    return out;
}

} // namespace hedgebot
