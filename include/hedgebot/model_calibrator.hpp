#pragma once

#include "bsm_pricer.hpp"
#include "market_data.hpp"
#include "merton_pricer.hpp"

#include <vector>

namespace hedgebot {

struct CalibrationSummary {
    bool success;
    double objective_value;
    int iterations;
};

class ModelCalibrator {
public:
    struct BSMResult {
        double sigma;
        CalibrationSummary summary;
    };

    struct MertonResult {
        MertonJump_Pricer::Parameters params;
        CalibrationSummary summary;
    };

    BSMResult calibrate_bsm_vol(
        const std::vector<OptionQuote>& surface,
        const MarketData& market_base,
        double min_sigma = 0.01,
        double max_sigma = 1.50,
        int grid_steps = 120) const;

    MertonResult calibrate_merton_params(
        const std::vector<OptionQuote>& surface,
        const MarketData& market_base,
        double min_lambda = 0.01,
        double max_lambda = 0.50,
        double min_mu_j = -0.25,
        double max_mu_j = 0.10,
        double min_sigma_j = 0.05,
        double max_sigma_j = 0.50,
        int lambda_steps = 12,
        int mu_steps = 12,
        int sigma_j_steps = 12) const;

private:
    static double bsm_objective(
        const std::vector<OptionQuote>& surface,
        const MarketData& market_base,
        double sigma);

    static double merton_objective(
        const std::vector<OptionQuote>& surface,
        const MarketData& market_base,
        const MertonJump_Pricer::Parameters& params);
};

} // namespace hedgebot
