#include "EnvironmentStore.h"
#include "TextUtil.h"
#include <fstream>
#include <sstream>

EnvironmentStore::EnvironmentStore(std::string filePath)
    : path(std::move(filePath))
{
}

std::vector<Environment> EnvironmentStore::Load() const
{
    std::ifstream input(path);
    std::vector<Environment> environments;

    if (!input)
    {
        return environments;
    }

    std::string line;
    while (std::getline(input, line))
    {
        const std::vector<std::string> columns = TextUtil::Split(line, '\t');
        if (columns.size() < 13)
        {
            continue;
        }

        Environment environment;
        environment.id = columns[0];
        environment.repository = columns[1];
        environment.branch = columns[2];
        environment.templateName = columns[3];
        environment.owner = columns[4];
        environment.status = ParseStatus(columns[5]);
        try
        {
            environment.createdAt = static_cast<std::time_t>(std::stoll(columns[6]));
            environment.expiresAt = static_cast<std::time_t>(std::stoll(columns[7]));
            environment.hourlyCostUsd = std::stod(columns[8]);
        }
        catch (const std::exception&)
        {
            continue; // Skip malformed environments
        }
        environment.workflowRunUrl = columns[9];
        environment.appUrl = columns[10];
        environment.databaseHost = columns[11];
        environment.databaseUser = columns[12];
        environment.databasePassword = columns.size() > 13 ? columns[13] : "";
        environment.logs = columns.size() > 14 ? TextUtil::Split(columns[14], '|') : std::vector<std::string>{};

        environments.push_back(environment);
    }

    return environments;
}

void EnvironmentStore::Save(const std::vector<Environment>& environments) const
{
    std::ofstream output(path, std::ios::trunc);

    for (const Environment& environment : environments)
    {
        output << environment.id << '\t'
               << environment.repository << '\t'
               << environment.branch << '\t'
               << environment.templateName << '\t'
               << environment.owner << '\t'
               << StatusName(environment.status) << '\t'
               << environment.createdAt << '\t'
               << environment.expiresAt << '\t'
               << environment.hourlyCostUsd << '\t'
               << environment.workflowRunUrl << '\t'
               << environment.appUrl << '\t'
               << environment.databaseHost << '\t'
               << environment.databaseUser << '\t'
               << environment.databasePassword << '\t'
               << TextUtil::Join(environment.logs, '|') << '\n';
    }
}

std::string EnvironmentStore::StatusName(EnvironmentStatus status)
{
    switch (status)
    {
    case EnvironmentStatus::Creating:
        return "Creating";
    case EnvironmentStatus::Running:
        return "Running";
    case EnvironmentStatus::Failed:
        return "Failed";
    case EnvironmentStatus::Destroying:
        return "Destroying";
    case EnvironmentStatus::Destroyed:
        return "Destroyed";
    case EnvironmentStatus::Expired:
        return "Expired";
    }

    return "Unknown";
}

EnvironmentStatus EnvironmentStore::ParseStatus(const std::string& status)
{
    if (status == "Running")
    {
        return EnvironmentStatus::Running;
    }
    if (status == "Failed")
    {
        return EnvironmentStatus::Failed;
    }
    if (status == "Destroying")
    {
        return EnvironmentStatus::Destroying;
    }
    if (status == "Destroyed")
    {
        return EnvironmentStatus::Destroyed;
    }
    if (status == "Expired")
    {
        return EnvironmentStatus::Expired;
    }

    return EnvironmentStatus::Creating;
}
