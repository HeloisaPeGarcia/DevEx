#include "CostEstimator.h"
#include <fstream>
#include <sstream>

double CostEstimator::GetMonthlyCostEstimate(const std::string& reportPath)
{
    std::ifstream file(reportPath);
    if (!file)
    {
        return -1.0;
    }
    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    size_t pos = content.find("\"totalMonthlyCost\":");
    if (pos != std::string::npos)
    {
        size_t start = content.find_first_of("0123456789", pos);
        if (start != std::string::npos)
        {
            size_t end = content.find_first_not_of("0123456789.", start);
            try
            {
                return std::stod(content.substr(start, end - start));
            }
            catch(...) {}
        }
    }
    return -1.0;
}
