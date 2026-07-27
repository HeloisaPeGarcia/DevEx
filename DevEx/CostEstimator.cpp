#include "CostEstimator.h"
#include <fstream>
#include <sstream>
#include "json.hpp"

double CostEstimator::GetMonthlyCostEstimate(const std::string& reportPath)
{
    std::ifstream file(reportPath);
    if (!file)
    {
        return -1.0;
    }
    
    try
    {
        nlohmann::json j;
        file >> j;
        if (j.contains("totalMonthlyCost"))
        {
            std::string costStr = j["totalMonthlyCost"].get<std::string>();
            return std::stod(costStr);
        }
    }
    catch (...) {}

    return -1.0;
}
