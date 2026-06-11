#pragma once

#include "Types.h"
#include <string>
#include <vector>

class EnvironmentStore
{
public:
    explicit EnvironmentStore(std::string filePath);
    std::vector<Environment> Load() const;
    void Save(const std::vector<Environment>& environments) const;
    static std::string StatusName(EnvironmentStatus status);

private:
    std::string path;
    static EnvironmentStatus ParseStatus(const std::string& status);
};
