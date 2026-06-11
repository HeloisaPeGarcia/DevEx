#include "GitProvider.h"

std::vector<Repository> SimulatedGitProvider::FetchRepositories(const UserSession&) const
{
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
