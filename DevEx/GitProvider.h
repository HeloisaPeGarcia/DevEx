#pragma once

#include "Types.h"
#include <vector>

class IGitProvider
{
public:
    virtual ~IGitProvider() = default;
    virtual std::vector<Repository> FetchRepositories(const UserSession& session) const = 0;
    virtual bool CanLaunch(const UserSession& session, const Repository& repository, const EnvironmentTemplate& selectedTemplate) const = 0;
};

class SimulatedGitProvider final : public IGitProvider
{
public:
    std::vector<Repository> FetchRepositories(const UserSession& session) const override;
    bool CanLaunch(const UserSession& session, const Repository& repository, const EnvironmentTemplate& selectedTemplate) const override;
};
