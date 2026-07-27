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
#include <future>

class OrbitDesktopApp
{
public:
    OrbitDesktopApp();
    void Run();
    
    // Headless / CLI execution methods
    bool RunHeadlessLaunch(const std::string& repoPath, const std::string& branch, const std::string& templateId, int ttlHours);
    bool RunHeadlessNuke(const std::string& envId);
    void RunHeadlessList() const;
    void RunDaemon();

private:
    std::unique_ptr<IGitProvider> gitProvider;
    std::unique_ptr<IEnvironmentOrchestrator> orchestrator;
    EnvironmentStore store;
    CredentialVault credentialVault;
    std::optional<UserSession> session;
    std::vector<Repository> repositories;
    std::vector<Environment> environments;
    mutable std::mutex environmentsMutex;
    std::vector<std::future<void>> backgroundTasks;

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
    std::string SelectEnvironment(const std::string& label) const;
    void ExpireOldEnvironments();
    int CountActiveEnvironments() const;
    double CurrentHourlyCost() const;
    static std::string RoleName(UserRole role);
    void ExtendTtl();
};
