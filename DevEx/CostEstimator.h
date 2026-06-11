#pragma once

#include <string>

class CostEstimator
{
public:
    static double GetMonthlyCostEstimate(const std::string& reportPath);
};
