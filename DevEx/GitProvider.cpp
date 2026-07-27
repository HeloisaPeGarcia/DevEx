#include "GitProvider.h"
#include "TextUtil.h"
#include <algorithm>
#include <iostream>

#include "json.hpp"
using json = nlohmann::json;

std::vector<Repository> ParseRepositories(const std::string& jsonStr)
{
    std::vector<Repository> repos;
    try {
        auto j = json::parse(jsonStr);
        for (const auto& item : j) {
            Repository repo;
            repo.owner = item.value("owner", json::object()).value("login", "");
            repo.name = item.value("name", "");
            if (!repo.owner.empty() && !repo.name.empty()) {
                repos.push_back(repo);
            }
        }
    } catch (...) {}
    return repos;
}

std::vector<std::string> ParseBranches(const std::string& jsonStr)
{
    std::vector<std::string> branches;
    try {
        auto j = json::parse(jsonStr);
        for (const auto& item : j) {
            std::string branch = item.value("name", "");
            if (!branch.empty() && std::find(branches.begin(), branches.end(), branch) == branches.end()) {
                branches.push_back(branch);
            }
        }
    } catch (...) {}
    return branches;
}

std::vector<Repository> SimulatedGitProvider::FetchRepositories(const UserSession& session) const
{
    // Try real API call first
    if (!session.token.empty())
    {
        std::string cmd = "curl -s -H \"Authorization: token " + TextUtil::EscapeShellArg(session.token) + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/user/repos\"";
        std::string response = TextUtil::ExecuteCommand(cmd);
        std::vector<Repository> realRepos = ParseRepositories(response);
        if (!realRepos.empty())
        {
            // For each repository, fetch its actual branches
            for (auto& repo : realRepos)
            {
                std::string branchCmd = "curl -s -H \"Authorization: token " + TextUtil::EscapeShellArg(session.token) + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/repos/" + repo.owner + "/" + repo.name + "/branches\"";
                std::string branchResponse = TextUtil::ExecuteCommand(branchCmd);
                repo.branches = ParseBranches(branchResponse);
                if (repo.branches.empty())
                {
                    repo.branches = { "main", "develop" }; // fallback default branches
                }
            }
            return realRepos;
        }
    }

    // Graceful fallback to simulated data
    return {
        {"octocat-dev", "sales-tracking-system", {"main", "develop", "feature/new-checkout", "bugfix/expired-coupon"}},
        {"octocat-dev", "customer-portal", {"main", "release/2.4", "feature/onboarding"}},
        {"orbit-labs", "checkout-service", {"main", "develop", "feature/recurring-pix"}},
        {"orbit-labs", "analytics-dashboard", {"main", "feature/funnel-chart", "hotfix/slow-load"}},
    };
}

bool SimulatedGitProvider::CanLaunch(const UserSession& session, const Repository& repository, const EnvironmentTemplate& selectedTemplate) const
{
    if (selectedTemplate.requiresAdmin && session.role != UserRole::Admin)
    {
        return false;
    }

    if (repository.owner == "orbit-labs" && session.role == UserRole::Developer)
    {
        return false;
    }

    return true;
}
