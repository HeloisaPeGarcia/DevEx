#pragma once

#include "Types.h"
#include <functional>
#include <future>
#include <vector>

class IEnvironmentOrchestrator
{
public:
    virtual ~IEnvironmentOrchestrator() = default;
    virtual Environment Launch(const UserSession& session, const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const = 0;
    virtual void Destroy(const Environment& environment, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const = 0;
};

class SimulatedEnvironmentOrchestrator final : public IEnvironmentOrchestrator
{
public:
    Environment Launch(const UserSession& session, const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const override;
    void Destroy(const Environment& environment, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const override;

private:
    mutable std::vector<std::future<void>> backgroundTasks;
};

