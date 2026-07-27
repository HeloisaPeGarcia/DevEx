#include "EnvironmentStore.h"
#include "TextUtil.h"
#include <fstream>
#include <sstream>
#include "json.hpp"

#ifdef _WIN32
#include <windows.h>
#endif

using json = nlohmann::json;

class ProcessFileLock
{
private:
#ifdef _WIN32
    HANDLE hLock = INVALID_HANDLE_VALUE;
#endif
public:
    ProcessFileLock(const std::string& path)
    {
#ifdef _WIN32
        std::wstring lockPath = std::wstring(path.begin(), path.end()) + L".lock";
        int retries = 50; // Try for up to 5 seconds
        while (retries-- > 0)
        {
            hLock = CreateFileW(lockPath.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_FLAG_DELETE_ON_CLOSE, NULL);
            if (hLock != INVALID_HANDLE_VALUE)
            {
                break;
            }
            Sleep(100);
        }
#endif
    }

    ~ProcessFileLock()
    {
#ifdef _WIN32
        if (hLock != INVALID_HANDLE_VALUE)
        {
            CloseHandle(hLock);
        }
#endif
    }
};

EnvironmentStore::EnvironmentStore(std::string filePath)
    : path(std::move(filePath))
{
}

std::vector<Environment> EnvironmentStore::Load() const
{
    ProcessFileLock lock(path);
    std::ifstream input(path);
    std::vector<Environment> environments;

    if (!input)
    {
        return environments;
    }

    std::string line;
    while (std::getline(input, line))
    {
        if (line.empty()) continue;
        try {
            auto j = json::parse(line);
            Environment environment;
            environment.id = j.value("id", "");
            environment.repository = j.value("repository", "");
            environment.branch = j.value("branch", "");
            environment.templateName = j.value("templateName", "");
            environment.owner = j.value("owner", "");
            environment.status = ParseStatus(j.value("status", "Creating"));
            environment.createdAt = static_cast<std::time_t>(j.value("createdAt", 0LL));
            environment.expiresAt = static_cast<std::time_t>(j.value("expiresAt", 0LL));
            environment.hourlyCostUsd = j.value("hourlyCostUsd", 0.0);
            environment.workflowRunUrl = j.value("workflowRunUrl", "");
            environment.appUrl = j.value("appUrl", "");
            environment.databaseHost = j.value("databaseHost", "");
            environment.databaseUser = j.value("databaseUser", "");
            environment.databasePassword = j.value("databasePassword", "");
            if (j.contains("logs") && j["logs"].is_array()) {
                environment.logs = j["logs"].get<std::vector<std::string>>();
            }
            if (!environment.id.empty()) {
                environments.push_back(environment);
            }
        } catch (...) {
            continue; // Skip malformed lines
        }
    }

    return environments;
}

void EnvironmentStore::Save(const std::vector<Environment>& environments) const
{
    ProcessFileLock lock(path);
    std::ofstream output(path, std::ios::trunc);

    for (const Environment& environment : environments)
    {
        json j;
        j["id"] = environment.id;
        j["repository"] = environment.repository;
        j["branch"] = environment.branch;
        j["templateName"] = environment.templateName;
        j["owner"] = environment.owner;
        j["status"] = StatusName(environment.status);
        j["createdAt"] = static_cast<long long>(environment.createdAt);
        j["expiresAt"] = static_cast<long long>(environment.expiresAt);
        j["hourlyCostUsd"] = environment.hourlyCostUsd;
        j["workflowRunUrl"] = environment.workflowRunUrl;
        j["appUrl"] = environment.appUrl;
        j["databaseHost"] = environment.databaseHost;
        j["databaseUser"] = environment.databaseUser;
        j["databasePassword"] = environment.databasePassword;
        j["logs"] = environment.logs;

        output << j.dump() << '\n';
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
