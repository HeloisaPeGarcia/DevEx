#include "GitProvider.h"
#include "TextUtil.h"
#include <algorithm>
#include <iostream>

std::vector<Repository> ParseRepositories(const std::string& json)
{
    std::vector<Repository> repos;
    size_t pos = 0;
    while (true)
    {
        pos = json.find("\"full_name\":", pos);
        if (pos == std::string::npos) break;

        pos = json.find("\"", pos + 12);
        if (pos == std::string::npos) break;

        size_t endPos = json.find("\"", pos + 1);
        if (endPos == std::string::npos) break;

        std::string fullName = json.substr(pos + 1, endPos - pos - 1);
        size_t slash = fullName.find('/');
        if (slash != std::string::npos)
        {
            Repository repo;
            repo.owner = fullName.substr(0, slash);
            repo.name = fullName.substr(slash + 1);
            repos.push_back(repo);
        }
        pos = endPos;
    }
    return repos;
}

std::vector<std::string> ParseBranches(const std::string& json)
{
    std::vector<std::string> branches;
    size_t pos = 0;
    while (true)
    {
        pos = json.find("\"name\":", pos);
        if (pos == std::string::npos) break;

        pos = json.find("\"", pos + 7);
        if (pos == std::string::npos) break;

        size_t endPos = json.find("\"", pos + 1);
        if (endPos == std::string::npos) break;

        std::string branch = json.substr(pos + 1, endPos - pos - 1);
        if (std::find(branches.begin(), branches.end(), branch) == branches.end())
        {
            branches.push_back(branch);
        }
        pos = endPos;
    }
    return branches;
}

std::vector<Repository> SimulatedGitProvider::FetchRepositories(const UserSession& session) const
{
    // Try real API call first
    if (!session.token.empty())
    {
        std::string cmd = "curl -s -H \"Authorization: token " + session.token + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/user/repos\"";
        std::string response = TextUtil::ExecuteCommand(cmd);
        std::vector<Repository> realRepos = ParseRepositories(response);
        if (!realRepos.empty())
        {
            // For each repository, fetch its actual branches
            for (auto& repo : realRepos)
            {
                std::string branchCmd = "curl -s -H \"Authorization: token " + session.token + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/repos/" + repo.owner + "/" + repo.name + "/branches\"";
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
