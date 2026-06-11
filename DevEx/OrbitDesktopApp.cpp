#include "OrbitDesktopApp.h"
#include "Terminal.h"
#include "TextUtil.h"
#include <iostream>
#include <iomanip>
#include <algorithm>
#include <thread>

using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

OrbitDesktopApp::OrbitDesktopApp()
    : gitProvider(std::make_unique<SimulatedGitProvider>()),
      orchestrator(std::make_unique<SimulatedEnvironmentOrchestrator>()),
      store("orbitdesktop.environments.tsv")
{
    templates = {
        {"web-postgres", "Web + PostgreSQL", "Web app hosting with temporary PostgreSQL instance", 0.42, false},
        {"api-redis", "API + Redis", "Backend API microservice with isolated temporary Redis", 0.36, false},
        {"fullstack", "Frontend + API + PostgreSQL", "Complete multi-tier stack for end-to-end testing", 0.78, false},
        {"microservice-k8s", "Kubernetes Namespace", "Dedicated namespace with HPA, secrets, and ingress ingress", 1.15, true},
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
        std::cout << "0. Exit\n\n";

        switch (Terminal::ReadOption("Choose an option: ", 0, 5))
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
        UserSession authenticated = credentialVault.Authenticate(token);

        if (!authenticated.username.empty())
        {
            session = authenticated;
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

    std::cout << "\nLaunch Summary:\n";
    std::cout << "  Repository: " << repository.owner << "/" << repository.name << "\n";
    std::cout << "  Branch:     " << branch << "\n";
    std::cout << "  Template:   " << selectedTemplate.name << "\n";
    std::cout << "  TTL:        " << ttlHours << "h\n";
    std::cout << "  Hourly Cost: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd << "\n";
    std::cout << "  Max Total Cost Estimate: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd * ttlHours << "\n";

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
    std::cout << "\nInfrastructure Templates:\n";
    for (std::size_t index = 0; index < templates.size(); ++index)
    {
        std::cout << index + 1 << ". " << templates[index].name
                  << " | US$ " << std::fixed << std::setprecision(2) << templates[index].hourlyCostUsd << "/h";

        if (templates[index].requiresAdmin)
        {
            std::cout << " [Requires Admin]";
        }

        std::cout << "\n";
    }
    std::cout << "0. Back\n\n";

    const int selected = Terminal::ReadOption("Select template: ", 0, static_cast<int>(templates.size()));
    if (selected == 0)
    {
        return {};
    }

    return templates[static_cast<std::size_t>(selected - 1)];
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
                  << " | Expires: " << Terminal::FormatTime(environment.expiresAt)
                  << " | Cost: US$ " << std::fixed << std::setprecision(2) << environment.hourlyCostUsd << "/h\n";

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

    const int selected = SelectEnvironment("Select environment to view logs: ");
    if (selected == 0)
    {
        return;
    }

    Environment environment;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        if (static_cast<std::size_t>(selected - 1) < environments.size())
        {
            environment = environments[static_cast<std::size_t>(selected - 1)];
        }
        else
        {
            return;
        }
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

    const int selected = SelectEnvironment("Select environment to destroy: ");
    if (selected == 0)
    {
        return;
    }

    bool alreadyFinalized = false;
    std::string envId;
    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        if (static_cast<std::size_t>(selected - 1) < environments.size())
        {
            const Environment& environment = environments[static_cast<std::size_t>(selected - 1)];
            if (environment.status == EnvironmentStatus::Destroyed || environment.status == EnvironmentStatus::Expired)
            {
                alreadyFinalized = true;
            }
            else
            {
                envId = environment.id;
            }
        }
        else
        {
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

    {
        std::lock_guard<std::mutex> lock(environmentsMutex);
        for (auto& env : environments)
        {
            if (env.id == envId)
            {
                env.status = EnvironmentStatus::Destroying;
                store.Save(environments);

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

                Environment envCopy = env;
                std::thread([this, envCopy, onUpdate]() {
                    orchestrator->Destroy(const_cast<Environment&>(envCopy), onUpdate);
                }).detach();

                break;
            }
        }
    }

    std::cout << "\nTeardown process dispatched in the background.\n";
    Terminal::Pause();
}

void OrbitDesktopApp::ShowTemplates() const
{
    Terminal::Header("Infrastructure Templates");

    for (const EnvironmentTemplate& selectedTemplate : templates)
    {
        std::cout << selectedTemplate.name << "\n";
        std::cout << "  ID:       " << selectedTemplate.id << "\n";
        std::cout << "  Usage:    " << selectedTemplate.description << "\n";
        std::cout << "  Cost:     US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd << "/h\n";
        std::cout << "  Requires: " << (selectedTemplate.requiresAdmin ? "Admin Access" : "Developer (or higher)") << "\n\n";
    }

    Terminal::Pause();
}

int OrbitDesktopApp::SelectEnvironment(const std::string& label) const
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
        return 0;
    }

    for (std::size_t index = 0; index < envsCopy.size(); ++index)
    {
        const Environment& environment = envsCopy[index];
        std::cout << index + 1 << ". " << environment.id
                  << " | " << EnvironmentStore::StatusName(environment.status)
                  << " | " << environment.repository
                  << " | " << environment.branch << "\n";
    }
    std::cout << "0. Back\n\n";

    return Terminal::ReadOption(label, 0, static_cast<int>(envsCopy.size()));
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
