#pragma once

#include "Types.h"
#include "GitProvider.h"
#include "Orchestrator.h"
#include "EnvironmentStore.h"
#include "CredentialVault.h"
#include <memory>
#include <vector>
#include <optional>
#include <mutex>

class OrbitDesktopApp
{
public:
    OrbitDesktopApp();
    void Run();

private:
    std::unique_ptr<IGitProvider> gitProvider;
    std::unique_ptr<IEnvironmentOrchestrator> orchestrator;
    EnvironmentStore store;
    CredentialVault credentialVault;
    std::optional<UserSession> session;
    std::vector<Repository> repositories;
    std::vector<Environment> environments;
    mutable std::mutex environmentsMutex;

    std::vector<EnvironmentTemplate> templates;

    bool Login();
    void OpenCatalog();
    void SelectBranchAndLaunch(const Repository& repository);
    EnvironmentTemplate SelectTemplate() const;
    static int SelectTtlHours();
    void Provision(const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours);
    void ShowDashboard() const;
    void ShowLogs() const;
    void NukeEnvironment();
    void ShowTemplates() const;
    int SelectEnvironment(const std::string& label) const;
    void ExpireOldEnvironments();
    int CountActiveEnvironments() const;
    double CurrentHourlyCost() const;
    static std::string RoleName(UserRole role);
};
