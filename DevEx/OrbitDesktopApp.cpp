#include "OrbitDesktopApp.h"
#include "Terminal.h"
#include "TextUtil.h"
#include "CostEstimator.h"
#include "NetworkManager.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>
#include <cstdlib>

using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

OrbitDesktopApp::OrbitDesktopApp()
    : gitProvider(std::make_unique<SimulatedGitProvider>()),
      orchestrator(std::make_unique<SimulatedEnvironmentOrchestrator>()),
      store([]() {
          const char* envPath = std::getenv("ORBIT_STORE_PATH");
          return envPath ? std::string(envPath) : "orbitdesktop.environments.tsv";
      }())
{
    templates = {
        {"web-postgres", "Web + PostgreSQL", "Web app hosting with temporary PostgreSQL instance", 0.42, false},
        {"api-redis", "API + Redis", "Backend API microservice with isolated temporary Redis", 0.36, false},
        {"fullstack", "Frontend + API + PostgreSQL", "Complete multi-tier stack for end-to-end testing", 0.78, false},
        {"microservice-k8s", "Kubernetes Namespace", "Dedicated namespace with HPA, secrets, and ingress ingress", 1.15, true},
        {"local-k8s", "Local Kubernetes", "Deploy directly to local Minikube/k3d cluster", 0.00, false},
    };
}

void OrbitDesktopApp::Run()
{
    Terminal::Header("Secure Authentication");

    if (!Login())
    {
        std::cout << "Failed to authenticate. Exiting.\n";
        return;
    }

    repositories = gitProvider->FetchRepositories(session.value());
    environments = store.Load();
    ExpireOldEnvironments();

    bool running = true;
    while (running)
    {
        Terminal::Header("Main Console Dashboard");
        std::cout << "User: " << session->username << " | Role: " << RoleName(session->role) << " | Token: " << session->tokenPreview << "\n";
        std::cout << "Active environments: " << CountActiveEnvironments() << " | Estimated hourly cost: US$ " << std::fixed << std::setprecision(2) << CurrentHourlyCost() << "\n\n";
        std::cout << "1. Project Catalog\n";
        std::cout << "2. Active Dashboard\n";
        std::cout << "3. View Environment Logs\n";
        std::cout << "4. Nuke: Destroy Environment\n";
        std::cout << "5. Infrastructure Templates\n";
        std::cout << "6. Extend TTL (+2 hours)\n";
        std::cout << "0. Exit\n\n";

        switch (Terminal::ReadOption("Choose an option: ", 0, 6))
        {
        case 1:
            OpenCatalog();
            break;
        case 2:
            ShowDashboard();
            break;
        case 3:
            ShowLogs();
            break;
        case 4:
            NukeEnvironment();
            break;
        case 5:
            ShowTemplates();
            break;
        case 6:
            ExtendTtl();
            break;
        case 0:
            running = false;
            break;
        }

        ExpireOldEnvironments();
        {
            std::lock_guard<std::mutex> lock(environmentsMutex);
            store.Save(environments);
        }
    }
}

bool OrbitDesktopApp::Login()
{
    for (int attempt = 1; attempt <= 3; ++attempt)
    {
        const std::string token = Terminal::ReadLine("Enter your GitHub/GitLab Personal Access Token: ");
        auto authenticated = credentialVault.Authenticate(token);

        if (authenticated)
        {
            session = *authenticated;
            std::cout << "\nToken accepted. In production, credentials will be stored securely via Windows Credential Manager.\n";
            std::this_thread::sleep_for(700ms);
            return true;
        }

        std::cout << "Invalid token format or length. Try again.\n\n";
    }

    return false;
}

void OrbitDesktopApp::OpenCatalog()
{
    Terminal::Header("Project Catalog");
    const std::string query = TextUtil::ToLower(Terminal::ReadLine("Search by name, owner, or leave empty to list all: "));

    std::vector<Repository> matches;
    for (const Repository& repository : repositories)
    {
        const std::string searchable = TextUtil::ToLower(repository.owner + "/" + repository.name);
        if (query.empty() || searchable.find(query) != std::string::npos)
        {
            matches.push_back(repository);
        }
    }

    if (matches.empty())
    {
        std::cout << "\nNo projects found matching the criteria.\n";
        Terminal::Pause();
        return;
    }

    std::cout << "\nMatching Projects:\n";
    for (std::size_t index = 0; index < matches.size(); ++index)
    {
        std::cout << index + 1 << ". " << matches[index].owner << "/" << matches[index].name << "\n";
    }
    std::cout << "0. Back\n\n";

    const int selectedProject = Terminal::ReadOption("Select project: ", 0, static_cast<int>(matches.size()));
    if (selectedProject == 0)
    {
        return;
    }

    SelectBranchAndLaunch(matches[static_cast<std::size_t>(selectedProject - 1)]);
}
void OrbitDesktopApp::SelectBranchAndLaunch(const Repository& repository)
{
    Terminal::Header("One-Click Provisioning");
    std::cout << "Project: " << repository.owner << "/" << repository.name << "\n\n";

    for (std::size_t index = 0; index < repository.branches.size(); ++index)
    {
        std::cout << index + 1 << ". " << repository.branches[index] << "\n";
    }
    std::cout << "0. Back\n\n";

    const int selectedBranch = Terminal::ReadOption("Select branch: ", 0, static_cast<int>(repository.branches.size()));
    if (selectedBranch == 0)
    {
        return;
    }

    const EnvironmentTemplate selectedTemplate = SelectTemplate();
    if (selectedTemplate.id.empty())
    {
        return;
    }

    if (!gitProvider->CanLaunch(*session, repository, selectedTemplate))
    {
        std::cout << "\nPermission denied. Your role is restricted from launching this infrastructure template on this repository.\n";
        Terminal::Pause();
        return;
    }

    const int ttlHours = SelectTtlHours();
    const std::string branch = repository.branches[static_cast<std::size_t>(selectedBranch - 1)];

    double infracostCost = CostEstimator::GetMonthlyCostEstimate("infracost-report.json");
    double monthlyEstimate = (infracostCost >= 0.0) ? infracostCost : (selectedTemplate.hourlyCostUsd * 24 * 30);

    std::cout << "\nLaunch Summary:\n";
    std::cout << "  Repository: " << repository.owner << "/" << repository.name << "\n";
    std::cout << "  Branch:     " << branch << "\n";
    std::cout << "  Template:   " << selectedTemplate.name << "\n";
    std::cout << "  TTL:        " << ttlHours << "h\n";
    
    if (infracostCost >= 0.0)
    {
        std::cout << "  Cost (Infracost monthly): US$ " << std::fixed << std::setprecision(2) << infracostCost << "\n";
    }
    else
    {
        std::cout << "  Hourly Cost: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd << "\n";
        std::cout << "  Max Total Cost Estimate: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd * ttlHours << "\n";
    }
    std::cout << "  Estimated Monthly Cost:  US$ " << std::fixed << std::setprecision(2) << monthlyEstimate << "\n";

    if (monthlyEstimate > 50.0 && session->role != UserRole::Admin)
    {
        std::cout << "\n[FinOps Policy Alert] Launch denied! Estimated monthly cost (US$ " 
                  << std::fixed << std::setprecision(2) << monthlyEstimate 
                  << ") exceeds developer limit of US$ 50.00. Admin role required.\n";
        Terminal::Pause();
        return;
    }

    const std::string confirmation = Terminal::ReadLine("\nLaunch Environment? (y/n): ");
    std::string cleanConf = TextUtil::ToLower(TextUtil::Trim(confirmation));
    if (cleanConf.empty() || (cleanConf[0] != 'y' && cleanConf != "yes" && cleanConf != "s" && cleanConf != "sim"))
    {
        return;
    }

    Provision(repository, branch, selectedTemplate, ttlHours);
}

EnvironmentTemplate OrbitDesktopApp::SelectTemplate() const
{
    std::vector<EnvironmentTemplate> availableTemplates;
    for (const auto& tpl : templates)
    {
        if (tpl.requiresAdmin && session->role != UserRole::Admin) continue;
        availableTemplates.push_back(tpl);
    }

    if (availableTemplates.empty())
    {
        std::cout << "No templates available for your role.\n";
        Terminal::Pause();
        return {};
    }

    std::cout << "\nInfrastructure Templates:\n";
    for (std::size_t index = 0; index < availableTemplates.size(); ++index)
    {
        std::cout << index + 1 << ". " << availableTemplates[index].name
                  << " | US$ " << std::fixed << std::setprecision(2) << availableTemplates[index].hourlyCostUsd << "/h\n";
    }
    std::cout << "0. Back\n\n";

    const int selected = Terminal::ReadOption("Select template: ", 0, static_cast<int>(availableTemplates.size()));
    if (selected == 0)
    {
        return {};
    }

    return availableTemplates[static_cast<std::size_t>(selected - 1)];
}

int OrbitDesktopApp::SelectTtlHours()
{
    std::cout << "\nAuto-cleanup lifetime (TTL):\n";
    std::cout << "1. 2h (Quick check)\n";
    std::cout << "2. 8h (Full workday)\n";
    std::cout << "3. 24h (Overnight run)\n\n";

    switch (Terminal::ReadOption("Select TTL option: ", 1, 3))
    {
    case 1:
        return 2;
    case 2:
        return 8;
    default:
        return 24;
    }
}

void OrbitDesktopApp::Provision(const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours)
{
    Terminal::Header("Provisioning Initiated");

    auto onUpdate = [this](const std::string& id, EnvironmentStatus status, const std::string& logLine) {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (auto& env : environments)
        {
            if (env.id == id)
            {
                env.status = status;
                if (!logLine.empty())
                {
                    env.logs.push_back(logLine);
                }
                store.Save(environments);
                break;
            }
        }
    };

    Environment environment;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        environment = orchestrator->Launch(*session, repository, branch, selectedTemplate, ttlHours, onUpdate);
        environments.push_back(environment);
        store.Save(environments);
    }

    if (selectedTemplate.id == "local-k8s")
    {
        std::string appUrl = environment.appUrl;
        if (appUrl.rfind("https://", 0) == 0)
        {
            appUrl = appUrl.substr(8);
        }
        NetworkManager::UpdateHostsEntry(appUrl, "127.0.0.1");
    }

    std::cout << "\nPreview environment build triggered in background.\n";
    std::cout << "You can monitor the provisioning logs under the Dashboard menu.\n";
    Terminal::Pause();
    ShowDashboard();
}

void OrbitDesktopApp::ShowDashboard() const
{
    Terminal::Header("Active Dashboard");

    std::vector<Environment> envsCopy;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        envsCopy = environments;
    }

    if (envsCopy.empty())
    {
        std::cout << "No environments deployed yet.\n";
        Terminal::Pause();
        return;
    }

    for (std::size_t index = 0; index < envsCopy.size(); ++index)
    {
        const Environment& environment = envsCopy[index];
        std::cout << index + 1 << ". " << environment.id << "\n";
        std::cout << "   Status:    " << EnvironmentStore::StatusName(environment.status)
                  << " | Repo: " << environment.repository
                  << " | Branch: " << environment.branch << "\n";
        std::cout << "   Template:  " << environment.templateName
                  << " | Expires: " << Terminal::FormatTime(environment.expiresAt);
        
        std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        if (environment.expiresAt > now) {
            long long diff = environment.expiresAt - now;
            std::cout << " (" << (diff / 3600) << "h " << ((diff % 3600) / 60) << "m remaining)";
        } else {
            std::cout << " (Expired)";
        }

        double maxMonthlyCost = environment.hourlyCostUsd * 24 * 30;
        std::cout << " | Cost: US$ " << std::fixed << std::setprecision(2) << environment.hourlyCostUsd << "/h (Max Monthly: US$ " << maxMonthlyCost << ")\n";

        if (environment.status == EnvironmentStatus::Running)
        {
            std::cout << "   App URL:   " << environment.appUrl << "\n";
            std::cout << "   Database:  " << environment.databaseHost << " | User: " << environment.databaseUser << " | Pwd: " << environment.databasePassword << "\n";
        }

        std::cout << "   Workflow:  " << environment.workflowRunUrl << "\n\n";
    }

    Terminal::Pause();
}

void OrbitDesktopApp::ShowLogs() const
{
    Terminal::Header("Environment Logs");

    const std::string selectedId = SelectEnvironment("Enter environment ID to view logs (or 0 to go back): ");
    if (selectedId == "0" || selectedId.empty())
    {
        return;
    }

    Environment environment;
    bool found = false;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (const auto& env : environments)
        {
            if (env.id == selectedId)
            {
                environment = env;
                found = true;
                break;
            }
        }
    }

    if (!found)
    {
        std::cout << "Environment not found.\n";
        Terminal::Pause();
        return;
    }

    std::cout << "\nEnvironment ID: " << environment.id << "\n";
    std::cout << "CI/CD Workflow: " << environment.workflowRunUrl << "\n\n";

    for (const std::string& line : environment.logs)
    {
        std::cout << line << "\n";
    }

    Terminal::Pause();
}

void OrbitDesktopApp::NukeEnvironment()
{
    Terminal::Header("Nuke Environment");

    const std::string selectedId = SelectEnvironment("Enter environment ID to destroy (or 0 to go back): ");
    if (selectedId == "0" || selectedId.empty())
    {
        return;
    }

    bool alreadyFinalized = false;
    std::string envId;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        bool found = false;
        for (const auto& env : environments)
        {
            if (env.id == selectedId)
            {
                found = true;
                if (env.status == EnvironmentStatus::Destroyed || env.status == EnvironmentStatus::Expired)
                {
                    alreadyFinalized = true;
                }
                else
                {
                    envId = env.id;
                }
                break;
            }
        }
        
        if (!found)
        {
            std::cout << "Environment not found.\n";
            Terminal::Pause();
            return;
        }
    }

    if (alreadyFinalized)
    {
        std::cout << "\nThis environment is already terminated.\n";
        Terminal::Pause();
        return;
    }

    std::cout << "\nEnvironment: " << envId << "\n";
    std::cout << "This action will trigger terraform destroy and purge all associated Kubernetes resources.\n";
    const std::string confirmation = Terminal::ReadLine("Confirm destruction? (y/n): ");
    
    std::string cleanConf = TextUtil::ToLower(TextUtil::Trim(confirmation));
    if (cleanConf.empty() || (cleanConf[0] != 'y' && cleanConf != "yes" && cleanConf != "y" && cleanConf != "sim" && cleanConf != "s"))
    {
        return;
    }

    bool found = false;
    Environment envCopy;

    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (auto& env : environments)
        {
            if (env.id == envId)
            {
                env.status = EnvironmentStatus::Destroying;
                store.Save(environments);
                envCopy = env;
                found = true;
                break;
            }
        }
    }

    if (found)
    {
        auto onUpdate = [this](const std::string& id, EnvironmentStatus status, const std::string& logLine) {
            std::lock_guard<std::mutex> callbackLock(environmentsMutex);
            for (auto& e : environments)
            {
                if (e.id == id)
                {
                    e.status = status;
                    if (!logLine.empty())
                    {
                        e.logs.push_back(logLine);
                    }
                    store.Save(environments);
                    break;
                }
            }
        };

        orchestrator->Destroy(envCopy, onUpdate);
    }

    std::cout << "\nTeardown process dispatched in the background.\n";
    Terminal::Pause();
}

void OrbitDesktopApp::ShowTemplates() const
{
    Terminal::Header("Infrastructure Templates");

    for (const EnvironmentTemplate& selectedTemplate : templates)
    {
        if (selectedTemplate.requiresAdmin && session->role != UserRole::Admin) continue;

        std::cout << selectedTemplate.name << "\n";
        std::cout << "  ID:       " << selectedTemplate.id << "\n";
        std::cout << "  Usage:    " << selectedTemplate.description << "\n";
        std::cout << "  Cost:     US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd << "/h\n\n";
    }

    Terminal::Pause();
}

std::string OrbitDesktopApp::SelectEnvironment(const std::string& label) const
{
    std::vector<Environment> envsCopy;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        envsCopy = environments;
    }

    if (envsCopy.empty())
    {
        std::cout << "No active environments found.\n";
        Terminal::Pause();
        return "0";
    }

    for (std::size_t index = 0; index < envsCopy.size(); ++index)
    {
        const Environment& environment = envsCopy[index];
        std::cout << "- " << environment.id
                  << " | " << EnvironmentStore::StatusName(environment.status)
                  << " | " << environment.repository
                  << " | " << environment.branch << "\n";
    }
    std::cout << "0. Back\n\n";

    return TextUtil::Trim(Terminal::ReadLine(label));
}

void OrbitDesktopApp::ExpireOldEnvironments()
{
    std::lock_guard<std::mutex> lock(environmentsMutex);
    const std::time_t now = Clock::to_time_t(Clock::now());

    for (Environment& environment : environments)
    {
        if ((environment.status == EnvironmentStatus::Running || environment.status == EnvironmentStatus::Creating) && environment.expiresAt <= now)
        {
            environment.status = EnvironmentStatus::Expired;
            environment.logs.push_back("TTL expired. Environment queued for automated teardown.");
        }
    }
}

int OrbitDesktopApp::CountActiveEnvironments() const
{
    std::lock_guard<std::mutex> lock(environmentsMutex);
    return static_cast<int>(std::count_if(environments.begin(), environments.end(), [](const Environment& environment)
    {
        return environment.status == EnvironmentStatus::Running || environment.status == EnvironmentStatus::Creating;
    }));
}

double OrbitDesktopApp::CurrentHourlyCost() const
{
    std::lock_guard<std::mutex> lock(environmentsMutex);
    double total = 0.0;

    for (const Environment& environment : environments)
    {
        if (environment.status == EnvironmentStatus::Running || environment.status == EnvironmentStatus::Creating)
        {
            total += environment.hourlyCostUsd;
        }
    }

    return total;
}

std::string OrbitDesktopApp::RoleName(UserRole role)
{
    switch (role)
    {
    case UserRole::Admin:
        return "Admin";
    case UserRole::Maintainer:
        return "Maintainer";
    case UserRole::Developer:
        return "Developer";
    }

    return "Unknown";
}

bool OrbitDesktopApp::RunHeadlessLaunch(const std::string& repoPath, const std::string& branch, const std::string& templateId, int ttlHours)
{
    const char* envToken = std::getenv("ORBIT_TOKEN");
    if (!envToken)
    {
        std::cerr << "{\"error\": \"Authentication failed: ORBIT_TOKEN environment variable not set.\"}\n";
        return false;
    }

    UserSession authenticated = credentialVault.Authenticate(envToken);
    if (authenticated.username.empty())
    {
        std::cerr << "{\"error\": \"Authentication failed: Invalid ORBIT_TOKEN.\"}\n";
        return false;
    }
    session = authenticated;

    environments = store.Load();

    repositories = gitProvider->FetchRepositories(*session);
    const Repository* targetRepo = nullptr;
    for (const auto& r : repositories)
    {
        if (r.owner + "/" + r.name == repoPath)
        {
            targetRepo = &r;
            break;
        }
    }

    if (!targetRepo)
    {
        std::cerr << "{\"error\": \"Repository not found or access denied: " << repoPath << "\"}\n";
        return false;
    }

    bool branchExists = false;
    for (const auto& b : targetRepo->branches)
    {
        if (b == branch)
        {
            branchExists = true;
            break;
        }
    }

    if (!branchExists)
    {
        std::cerr << "{\"error\": \"Branch not found: " << branch << "\"}\n";
        return false;
    }

    const EnvironmentTemplate* targetTemplate = nullptr;
    for (const auto& t : templates)
    {
        if (t.id == templateId)
        {
            targetTemplate = &t;
            break;
        }
    }

    if (!targetTemplate)
    {
        std::cerr << "{\"error\": \"Template not found: " << templateId << "\"}\n";
        return false;
    }

    if (!gitProvider->CanLaunch(*session, *targetRepo, *targetTemplate))
    {
        std::cerr << "{\"error\": \"Permission denied for template " << templateId << "\"}\n";
        return false;
    }

    auto onUpdate = [this](const std::string& id, EnvironmentStatus status, const std::string& logLine) {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (auto& env : environments)
        {
            if (env.id == id)
            {
                env.status = status;
                if (!logLine.empty())
                {
                    env.logs.push_back(logLine);
                }
                store.Save(environments);
                break;
            }
        }
    };

    Environment environment;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        environment = orchestrator->Launch(*session, *targetRepo, branch, *targetTemplate, ttlHours, onUpdate);
        environments.push_back(environment);
        store.Save(environments);
    }

    std::cout << "{\n"
              << "  \"id\": \"" << environment.id << "\",\n"
              << "  \"status\": \"" << EnvironmentStore::StatusName(environment.status) << "\",\n"
              << "  \"repository\": \"" << environment.repository << "\",\n"
              << "  \"branch\": \"" << environment.branch << "\",\n"
              << "  \"appUrl\": \"" << environment.appUrl << "\"\n"
              << "}\n";

    return true;
}

bool OrbitDesktopApp::RunHeadlessNuke(const std::string& envId)
{
    const char* envToken = std::getenv("ORBIT_TOKEN");
    if (!envToken)
    {
        std::cerr << "{\"error\": \"Authentication failed: ORBIT_TOKEN environment variable not set.\"}\n";
        return false;
    }

    UserSession authenticated = credentialVault.Authenticate(envToken);
    if (authenticated.username.empty())
    {
        std::cerr << "{\"error\": \"Authentication failed: Invalid ORBIT_TOKEN.\"}\n";
        return false;
    }
    session = authenticated;

    environments = store.Load();

    Environment envCopy;
    bool found = false;

    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (auto& env : environments)
        {
            if (env.id == envId)
            {
                if (env.status == EnvironmentStatus::Destroyed || env.status == EnvironmentStatus::Expired)
                {
                    std::cerr << "{\"error\": \"Environment already terminated.\"}\n";
                    return false;
                }

                env.status = EnvironmentStatus::Destroying;
                store.Save(environments);
                envCopy = env;
                found = true;
                break;
            }
        }
    }

    if (found)
    {
        auto onUpdate = [this](const std::string& id, EnvironmentStatus status, const std::string& logLine) {
            std::lock_guard<std::mutex> callbackLock(environmentsMutex);
            for (auto& e : environments)
            {
                if (e.id == id)
                {
                    e.status = status;
                    if (!logLine.empty())
                    {
                        e.logs.push_back(logLine);
                    }
                    store.Save(environments);
                    break;
                }
            }
        };

        orchestrator->Destroy(envCopy, onUpdate);

        std::cout << "{\n"
                  << "  \"id\": \"" << envId << "\",\n"
                  << "  \"status\": \"Destroying\"\n"
                  << "}\n";
        return true;
    }

    std::cerr << "{\"error\": \"Environment not found: " << envId << "\"}\n";
    return false;
}

void OrbitDesktopApp::RunHeadlessList() const
{
    std::vector<Environment> envs = store.Load();
    std::cout << "[\n";
    for (std::size_t i = 0; i < envs.size(); ++i)
    {
        const auto& env = envs[i];
        std::cout << "  {\n"
                  << "    \"id\": \"" << env.id << "\",\n"
                  << "    \"repository\": \"" << env.repository << "\",\n"
                  << "    \"branch\": \"" << env.branch << "\",\n"
                  << "    \"template\": \"" << env.templateName << "\",\n"
                  << "    \"status\": \"" << EnvironmentStore::StatusName(env.status) << "\",\n"
                  << "    \"appUrl\": \"" << env.appUrl << "\"\n"
                  << "  }" << (i + 1 < envs.size() ? "," : "") << "\n";
    }
    std::cout << "]\n";
}

void OrbitDesktopApp::RunDaemon()
{
    std::cout << "[OrbitDaemon] Initializing headless daemon service...\n";
    std::cout << "[OrbitDaemon] Monitoring active environments for automatic TTL expiration...\n";

    int backoffSeconds = 5;

    while (true)
    {
        try
        {
            auto loadedEnvs = store.Load();
            std::lock_guard<std::mutex> lock(environmentsMutex);
            environments = loadedEnvs;
            backoffSeconds = 5;
        }
        catch (const std::exception& e)
        {
            std::cerr << "[OrbitDaemon] Failed to read database: " << e.what() << ". Retrying in " << backoffSeconds << "s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(backoffSeconds));
            backoffSeconds = std::min(60, backoffSeconds * 2);
            continue;
        }
        catch (...)
        {
            std::cerr << "[OrbitDaemon] Unknown error reading database. Retrying in " << backoffSeconds << "s...\n";
            std::this_thread::sleep_for(std::chrono::seconds(backoffSeconds));
            backoffSeconds = std::min(60, backoffSeconds * 2);
            continue;
        }

        const std::time_t now = Clock::to_time_t(Clock::now());
        bool changesMade = false;
        std::vector<Environment> environmentsToDestroy;

        {
            std::lock_guard<std::mutex> lock(environmentsMutex);
            for (auto& env : environments)
            {
                if ((env.status == EnvironmentStatus::Running || env.status == EnvironmentStatus::Creating) && env.expiresAt <= now)
                {
                    std::cout << "[OrbitDaemon] Environment " << env.id << " expired. Triggering auto-teardown.\n";
                    env.status = EnvironmentStatus::Destroying;
                    env.logs.push_back("TTL expired. Automatic daemon cleanup initiated.");
                    changesMade = true;
                    environmentsToDestroy.push_back(env);
                }
            }

            if (changesMade)
            {
                store.Save(environments);
            }
        }

        for (const auto& envToDestroy : environmentsToDestroy)
        {
            auto onUpdate = [this](const std::string& id, EnvironmentStatus status, const std::string& logLine) {
                std::lock_guard<std::mutex> callbackLock(environmentsMutex);
                for (auto& e : environments)
                {
                    if (e.id == id)
                    {
                        e.status = status;
                        if (!logLine.empty())
                        {
                            e.logs.push_back(logLine);
                        }
                        store.Save(environments);
                        break;
                    }
                }
            };
            orchestrator->Destroy(envToDestroy, onUpdate);
        }

        std::this_thread::sleep_for(5s);
    }
}

void OrbitDesktopApp::ExtendTtl()
{
    Terminal::Header("Extend TTL (+2 hours)");
    const std::string selectedId = SelectEnvironment("Enter environment ID to extend TTL (or 0 to go back): ");
    if (selectedId == "0" || selectedId.empty()) return;

    std::lock_guard<std::mutex> lock(environmentsMutex);
    bool found = false;
    for (auto& env : environments)
    {
        if (env.id == selectedId)
        {
            found = true;
            if (env.status != EnvironmentStatus::Running && env.status != EnvironmentStatus::Creating)
            {
                std::cout << "\nOnly active environments can have their TTL extended.\n";
                Terminal::Pause();
                return;
            }

            std::time_t newExpires = env.expiresAt + (2 * 60 * 60);
            if (newExpires - env.createdAt > (48 * 60 * 60))
            {
                std::cout << "\nMaximum total TTL (48 hours) reached. Cannot extend further.\n";
                Terminal::Pause();
                return;
            }

            env.expiresAt = newExpires;
            store.Save(environments);
            std::cout << "\nTTL extended successfully. New expiration: " << Terminal::FormatTime(env.expiresAt) << "\n";
            Terminal::Pause();
            break;
        }
    }
    
    if (!found)
    {
        std::cout << "Environment not found.\n";
        Terminal::Pause();
    }
}
