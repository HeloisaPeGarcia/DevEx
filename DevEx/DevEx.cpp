// OrbitDesktop - prototipo C++ para gerenciar ambientes efemeros de desenvolvimento.
// O projeto permanece como aplicacao console para respeitar o formato Visual Studio atual.

#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

enum class EnvironmentStatus
{
    Creating,
    Running,
    Failed,
    Destroying,
    Destroyed,
    Expired
};

enum class UserRole
{
    Developer,
    Maintainer,
    Admin
};

struct Repository
{
    std::string owner;
    std::string name;
    std::vector<std::string> branches;
};

struct EnvironmentTemplate
{
    std::string id;
    std::string name;
    std::string description;
    double hourlyCostUsd = 0.0;
    bool requiresAdmin = false;
};

struct UserSession
{
    std::string username;
    UserRole role = UserRole::Developer;
    std::string tokenPreview;
};

struct Environment
{
    std::string id;
    std::string repository;
    std::string branch;
    std::string templateName;
    std::string owner;
    EnvironmentStatus status = EnvironmentStatus::Creating;
    std::time_t createdAt = 0;
    std::time_t expiresAt = 0;
    double hourlyCostUsd = 0.0;
    std::string workflowRunUrl;
    std::string appUrl;
    std::string databaseHost;
    std::string databaseUser;
    std::string databasePassword;
    std::vector<std::string> logs;
};

class Text
{
public:
    static std::string ToLower(std::string value)
    {
        for (char& character : value)
        {
            character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
        }

        return value;
    }

    static std::string Slug(const std::string& value)
    {
        std::string normalized;

        for (const char character : value)
        {
            if (std::isalnum(static_cast<unsigned char>(character)))
            {
                normalized += static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
            }
            else if (!normalized.empty() && normalized.back() != '-')
            {
                normalized += '-';
            }
        }

        while (!normalized.empty() && normalized.back() == '-')
        {
            normalized.pop_back();
        }

        return normalized.empty() ? "value" : normalized;
    }

    static std::string Join(const std::vector<std::string>& values, char delimiter)
    {
        std::ostringstream output;

        for (std::size_t index = 0; index < values.size(); ++index)
        {
            if (index > 0)
            {
                output << delimiter;
            }

            output << values[index];
        }

        return output.str();
    }

    static std::vector<std::string> Split(const std::string& value, char delimiter)
    {
        std::vector<std::string> parts;
        std::string current;
        std::istringstream input(value);

        while (std::getline(input, current, delimiter))
        {
            parts.push_back(current);
        }

        return parts;
    }
};

class Terminal
{
public:
    static void Clear()
    {
        std::cout << "\x1B[2J\x1B[H";
    }

    static void Pause()
    {
        std::cout << "\nPressione Enter para continuar...";
        std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
    }

    static void Header(const std::string& subtitle)
    {
        Clear();
        std::cout << "============================================================\n";
        std::cout << " OrbitDesktop\n";
        std::cout << " Ambientes temporarios com GitHub Actions, Terraform e K8s\n";
        std::cout << "============================================================\n";
        std::cout << subtitle << "\n\n";
    }

    static int ReadOption(const std::string& label, int minimum, int maximum)
    {
        while (true)
        {
            std::cout << label;

            int option = minimum - 1;
            if (std::cin >> option && option >= minimum && option <= maximum)
            {
                std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
                return option;
            }

            std::cin.clear();
            std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
            std::cout << "Opcao invalida. Tente novamente.\n";
        }
    }

    static std::string ReadLine(const std::string& label)
    {
        std::cout << label;

        std::string value;
        std::getline(std::cin, value);
        return value;
    }

    static std::string FormatTime(std::time_t value)
    {
        if (value <= 0)
        {
            return "-";
        }

        std::tm localTime{};
        localtime_s(&localTime, &value);

        std::ostringstream output;
        output << std::put_time(&localTime, "%d/%m/%Y %H:%M");
        return output.str();
    }

    static std::string ProgressBar(int progress)
    {
        constexpr int width = 30;
        const int filled = (progress * width) / 100;

        std::string bar;
        for (int index = 0; index < width; ++index)
        {
            bar += index < filled ? '#' : '.';
        }

        return bar;
    }
};

class CredentialVault
{
public:
    UserSession Authenticate(const std::string& personalAccessToken) const
    {
        if (personalAccessToken.size() < 12)
        {
            return {};
        }

        UserSession session;
        session.username = "joao";
        session.tokenPreview = personalAccessToken.substr(0, 4) + "..." + personalAccessToken.substr(personalAccessToken.size() - 4);

        if (personalAccessToken.find("admin") != std::string::npos)
        {
            session.role = UserRole::Admin;
        }
        else if (personalAccessToken.find("maintainer") != std::string::npos)
        {
            session.role = UserRole::Maintainer;
        }

        return session;
    }
};

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
    std::vector<Repository> FetchRepositories(const UserSession&) const override
    {
        return {
            {"joao-dev", "meu-sistema-de-vendas", {"main", "develop", "feature/nova-tela", "bugfix/cupom-expirado"}},
            {"joao-dev", "portal-cliente", {"main", "release/2.4", "feature/onboarding"}},
            {"orbit-labs", "checkout-service", {"main", "develop", "feature/pix-recorrente"}},
            {"orbit-labs", "analytics-dashboard", {"main", "feature/funil-vendas", "hotfix/cards-lentos"}},
        };
    }

    bool CanLaunch(const UserSession& session, const Repository& repository, const EnvironmentTemplate& selectedTemplate) const override
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
};

class EnvironmentStore
{
public:
    explicit EnvironmentStore(std::string filePath)
        : path(std::move(filePath))
    {
    }

    std::vector<Environment> Load() const
    {
        std::ifstream input(path);
        std::vector<Environment> environments;

        if (!input)
        {
            return environments;
        }

        std::string line;
        while (std::getline(input, line))
        {
            const std::vector<std::string> columns = Text::Split(line, '\t');
            if (columns.size() < 13)
            {
                continue;
            }

            Environment environment;
            environment.id = columns[0];
            environment.repository = columns[1];
            environment.branch = columns[2];
            environment.templateName = columns[3];
            environment.owner = columns[4];
            environment.status = ParseStatus(columns[5]);
            environment.createdAt = static_cast<std::time_t>(std::stoll(columns[6]));
            environment.expiresAt = static_cast<std::time_t>(std::stoll(columns[7]));
            environment.hourlyCostUsd = std::stod(columns[8]);
            environment.workflowRunUrl = columns[9];
            environment.appUrl = columns[10];
            environment.databaseHost = columns[11];
            environment.databaseUser = columns[12];
            environment.databasePassword = columns.size() > 13 ? columns[13] : "";
            environment.logs = columns.size() > 14 ? Text::Split(columns[14], '|') : std::vector<std::string>{};

            environments.push_back(environment);
        }

        return environments;
    }

    void Save(const std::vector<Environment>& environments) const
    {
        std::ofstream output(path, std::ios::trunc);

        for (const Environment& environment : environments)
        {
            output << environment.id << '\t'
                   << environment.repository << '\t'
                   << environment.branch << '\t'
                   << environment.templateName << '\t'
                   << environment.owner << '\t'
                   << StatusName(environment.status) << '\t'
                   << environment.createdAt << '\t'
                   << environment.expiresAt << '\t'
                   << environment.hourlyCostUsd << '\t'
                   << environment.workflowRunUrl << '\t'
                   << environment.appUrl << '\t'
                   << environment.databaseHost << '\t'
                   << environment.databaseUser << '\t'
                   << environment.databasePassword << '\t'
                   << Text::Join(environment.logs, '|') << '\n';
        }
    }

    static std::string StatusName(EnvironmentStatus status)
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

private:
    std::string path;

    static EnvironmentStatus ParseStatus(const std::string& status)
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
};

class IEnvironmentOrchestrator
{
public:
    virtual ~IEnvironmentOrchestrator() = default;
    virtual Environment Launch(const UserSession& session, const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours) const = 0;
    virtual void Destroy(Environment& environment) const = 0;
};

class SimulatedEnvironmentOrchestrator final : public IEnvironmentOrchestrator
{
public:
    Environment Launch(const UserSession& session, const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours) const override
    {
        const std::string repoSlug = Text::Slug(repository.name);
        const std::string branchSlug = Text::Slug(branch);
        const std::string templateSlug = Text::Slug(selectedTemplate.name);
        const std::time_t now = Clock::to_time_t(Clock::now());
        const std::string environmentSlug = repoSlug + "-" + branchSlug + "-" + templateSlug;

        Environment environment;
        environment.id = "env-" + environmentSlug;
        environment.repository = repository.owner + "/" + repository.name;
        environment.branch = branch;
        environment.templateName = selectedTemplate.name;
        environment.owner = session.username;
        environment.status = EnvironmentStatus::Creating;
        environment.createdAt = now;
        environment.expiresAt = now + (ttlHours * 60 * 60);
        environment.hourlyCostUsd = selectedTemplate.hourlyCostUsd;
        environment.workflowRunUrl = "https://github.com/" + environment.repository + "/actions/runs/simulated-" + std::to_string(now);
        environment.appUrl = "https://" + environmentSlug + ".preview.orbitdesktop.local";
        environment.databaseHost = environmentSlug + ".db.preview.orbitdesktop.local:5432";
        environment.databaseUser = "orbit_" + branchSlug;
        environment.databasePassword = "tmp_" + std::to_string(environmentSlug.size() * 7919);

        const std::vector<std::string> steps = {
            "Validando permissoes e escopos do token",
            "POST /repos/" + environment.repository + "/dispatches",
            "Evento repository_dispatch recebido pelo GitHub Actions",
            "Clonando branch " + branch,
            "Construindo imagem Docker",
            "Executando terraform plan",
            "Executando terraform apply",
            "Criando namespace Kubernetes isolado",
            "Provisionando banco temporario",
            "Aplicando manifests com kubectl apply",
            "Aguardando health checks",
            "Publicando URL e credenciais temporarias",
        };

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const int progress = static_cast<int>(((index + 1) * 100) / steps.size());
            const std::string line = "[" + Terminal::ProgressBar(progress) + "] " + std::to_string(progress) + "%  " + steps[index] + "...";
            environment.logs.push_back(line);
            std::cout << line << "\n";
            std::this_thread::sleep_for(250ms);
        }

        environment.status = EnvironmentStatus::Running;
        environment.logs.push_back("Ambiente pronto: " + environment.appUrl);
        return environment;
    }

    void Destroy(Environment& environment) const override
    {
        environment.status = EnvironmentStatus::Destroying;

        const std::vector<std::string> steps = {
            "Enviando evento orbitdesktop.destroy",
            "Removendo ingress e services",
            "Derrubando pods do namespace isolado",
            "Destruindo banco de dados temporario",
            "Executando terraform destroy",
            "Liberando recursos alocados",
        };

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const int progress = static_cast<int>(((index + 1) * 100) / steps.size());
            const std::string line = "[" + Terminal::ProgressBar(progress) + "] " + std::to_string(progress) + "%  " + steps[index] + "...";
            environment.logs.push_back(line);
            std::cout << line << "\n";
            std::this_thread::sleep_for(250ms);
        }

        environment.status = EnvironmentStatus::Destroyed;
        environment.logs.push_back("Ambiente destruido com sucesso.");
    }
};

class OrbitDesktopApp
{
public:
    OrbitDesktopApp()
        : gitProvider(std::make_unique<SimulatedGitProvider>()),
          orchestrator(std::make_unique<SimulatedEnvironmentOrchestrator>()),
          store("orbitdesktop.environments.tsv")
    {
    }

    void Run()
    {
        Terminal::Header("Autenticacao segura");

        if (!Login())
        {
            std::cout << "Nao foi possivel autenticar. Encerrando.\n";
            return;
        }

        repositories = gitProvider->FetchRepositories(session.value());
        environments = store.Load();
        ExpireOldEnvironments();

        bool running = true;
        while (running)
        {
            Terminal::Header("Painel principal");
            std::cout << "Usuario: " << session->username << " | Perfil: " << RoleName(session->role) << " | Token: " << session->tokenPreview << "\n";
            std::cout << "Ambientes ativos: " << CountActiveEnvironments() << " | Custo estimado/h: US$ " << std::fixed << std::setprecision(2) << CurrentHourlyCost() << "\n\n";
            std::cout << "1. Catalogo de projetos\n";
            std::cout << "2. Dashboard de ambientes\n";
            std::cout << "3. Logs de um ambiente\n";
            std::cout << "4. Nuke: destruir ambiente\n";
            std::cout << "5. Templates de infraestrutura\n";
            std::cout << "0. Sair\n\n";

            switch (Terminal::ReadOption("Escolha uma opcao: ", 0, 5))
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
            store.Save(environments);
        }
    }

private:
    std::unique_ptr<IGitProvider> gitProvider;
    std::unique_ptr<IEnvironmentOrchestrator> orchestrator;
    EnvironmentStore store;
    CredentialVault credentialVault;
    std::optional<UserSession> session;
    std::vector<Repository> repositories;
    std::vector<Environment> environments;
    std::vector<EnvironmentTemplate> templates = {
        {"web-postgres", "Web + PostgreSQL", "App web com banco PostgreSQL temporario", 0.42, false},
        {"api-redis", "API + Redis", "Servico backend com Redis efemero", 0.36, false},
        {"fullstack", "Frontend + API + PostgreSQL", "Stack completa para validacao de feature", 0.78, false},
        {"microservice-k8s", "Microservico Kubernetes", "Namespace dedicado com HPA, secrets e ingress", 1.15, true},
    };

    bool Login()
    {
        for (int attempt = 1; attempt <= 3; ++attempt)
        {
            const std::string token = Terminal::ReadLine("Informe seu Personal Access Token GitHub/GitLab: ");
            UserSession authenticated = credentialVault.Authenticate(token);

            if (!authenticated.username.empty())
            {
                session = authenticated;
                std::cout << "\nToken aceito. Em produto, ele seria guardado no Windows Credential Manager.\n";
                std::this_thread::sleep_for(700ms);
                return true;
            }

            std::cout << "Token invalido. Use ao menos 12 caracteres para este prototipo.\n\n";
        }

        return false;
    }

    void OpenCatalog()
    {
        Terminal::Header("Catalogo de projetos");
        const std::string query = Text::ToLower(Terminal::ReadLine("Busca por nome, owner ou deixe vazio para listar tudo: "));

        std::vector<Repository> matches;
        for (const Repository& repository : repositories)
        {
            const std::string searchable = Text::ToLower(repository.owner + "/" + repository.name);
            if (query.empty() || searchable.find(query) != std::string::npos)
            {
                matches.push_back(repository);
            }
        }

        if (matches.empty())
        {
            std::cout << "\nNenhum projeto encontrado.\n";
            Terminal::Pause();
            return;
        }

        std::cout << "\nProjetos encontrados:\n";
        for (std::size_t index = 0; index < matches.size(); ++index)
        {
            std::cout << index + 1 << ". " << matches[index].owner << "/" << matches[index].name << "\n";
        }
        std::cout << "0. Voltar\n\n";

        const int selectedProject = Terminal::ReadOption("Selecione o projeto: ", 0, static_cast<int>(matches.size()));
        if (selectedProject == 0)
        {
            return;
        }

        SelectBranchAndLaunch(matches[static_cast<std::size_t>(selectedProject - 1)]);
    }

    void SelectBranchAndLaunch(const Repository& repository)
    {
        Terminal::Header("Provisionamento em 1-clique");
        std::cout << "Projeto: " << repository.owner << "/" << repository.name << "\n\n";

        for (std::size_t index = 0; index < repository.branches.size(); ++index)
        {
            std::cout << index + 1 << ". " << repository.branches[index] << "\n";
        }
        std::cout << "0. Voltar\n\n";

        const int selectedBranch = Terminal::ReadOption("Selecione a branch: ", 0, static_cast<int>(repository.branches.size()));
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
            std::cout << "\nPermissao negada. Seu perfil nao pode criar este tipo de ambiente para este repositorio.\n";
            Terminal::Pause();
            return;
        }

        const int ttlHours = SelectTtlHours();
        const std::string branch = repository.branches[static_cast<std::size_t>(selectedBranch - 1)];

        std::cout << "\nResumo:\n";
        std::cout << "  Repositorio: " << repository.owner << "/" << repository.name << "\n";
        std::cout << "  Branch: " << branch << "\n";
        std::cout << "  Template: " << selectedTemplate.name << "\n";
        std::cout << "  TTL: " << ttlHours << "h\n";
        std::cout << "  Custo estimado: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd * ttlHours << "\n";

        const std::string confirmation = Terminal::ReadLine("\nLaunch Environment? (s/n): ");
        if (confirmation.empty() || std::tolower(static_cast<unsigned char>(confirmation[0])) != 's')
        {
            return;
        }

        Provision(repository, branch, selectedTemplate, ttlHours);
    }

    EnvironmentTemplate SelectTemplate() const
    {
        std::cout << "\nTemplates de infraestrutura:\n";
        for (std::size_t index = 0; index < templates.size(); ++index)
        {
            std::cout << index + 1 << ". " << templates[index].name
                      << " | US$ " << std::fixed << std::setprecision(2) << templates[index].hourlyCostUsd << "/h";

            if (templates[index].requiresAdmin)
            {
                std::cout << " | Admin";
            }

            std::cout << "\n";
        }
        std::cout << "0. Voltar\n\n";

        const int selected = Terminal::ReadOption("Selecione o template: ", 0, static_cast<int>(templates.size()));
        if (selected == 0)
        {
            return {};
        }

        return templates[static_cast<std::size_t>(selected - 1)];
    }

    static int SelectTtlHours()
    {
        std::cout << "\nTempo de vida automatico:\n";
        std::cout << "1. 2h\n";
        std::cout << "2. 8h\n";
        std::cout << "3. 24h\n\n";

        switch (Terminal::ReadOption("Selecione o TTL: ", 1, 3))
        {
        case 1:
            return 2;
        case 2:
            return 8;
        default:
            return 24;
        }
    }

    void Provision(const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours)
    {
        Terminal::Header("Monitoramento em tempo real");
        Environment environment = orchestrator->Launch(*session, repository, branch, selectedTemplate, ttlHours);

        environments.push_back(environment);
        store.Save(environments);

        std::cout << "\nNotificacao Windows: Ambiente Pronto!\n";
        std::cout << "O OrbitDesktop abriria o dashboard ao clicar na notificacao.\n";
        Terminal::Pause();
        ShowDashboard();
    }

    void ShowDashboard() const
    {
        Terminal::Header("Dashboard de ambientes");

        if (environments.empty())
        {
            std::cout << "Nenhum ambiente criado ainda.\n";
            Terminal::Pause();
            return;
        }

        for (std::size_t index = 0; index < environments.size(); ++index)
        {
            const Environment& environment = environments[index];
            std::cout << index + 1 << ". " << environment.id << "\n";
            std::cout << "   Status: " << EnvironmentStore::StatusName(environment.status)
                      << " | Repo: " << environment.repository
                      << " | Branch: " << environment.branch << "\n";
            std::cout << "   Template: " << environment.templateName
                      << " | Expira: " << Terminal::FormatTime(environment.expiresAt)
                      << " | US$ " << std::fixed << std::setprecision(2) << environment.hourlyCostUsd << "/h\n";

            if (environment.status == EnvironmentStatus::Running)
            {
                std::cout << "   App: " << environment.appUrl << "\n";
                std::cout << "   DB: " << environment.databaseHost << " | User: " << environment.databaseUser << " | Senha: " << environment.databasePassword << "\n";
            }

            std::cout << "   Workflow: " << environment.workflowRunUrl << "\n\n";
        }

        Terminal::Pause();
    }

    void ShowLogs() const
    {
        Terminal::Header("Logs de ambiente");

        const int selected = SelectEnvironment("Selecione o ambiente para ver logs: ");
        if (selected == 0)
        {
            return;
        }

        const Environment& environment = environments[static_cast<std::size_t>(selected - 1)];
        std::cout << "\n" << environment.id << "\n";
        std::cout << "Workflow: " << environment.workflowRunUrl << "\n\n";

        for (const std::string& line : environment.logs)
        {
            std::cout << line << "\n";
        }

        Terminal::Pause();
    }

    void NukeEnvironment()
    {
        Terminal::Header("Nuke");

        const int selected = SelectEnvironment("Selecione o ambiente para destruir: ");
        if (selected == 0)
        {
            return;
        }

        Environment& environment = environments[static_cast<std::size_t>(selected - 1)];
        if (environment.status == EnvironmentStatus::Destroyed || environment.status == EnvironmentStatus::Expired)
        {
            std::cout << "\nEste ambiente ja esta finalizado.\n";
            Terminal::Pause();
            return;
        }

        std::cout << "\nAmbiente: " << environment.id << "\n";
        std::cout << "Esta acao executara terraform destroy e removera os recursos Kubernetes.\n";
        const std::string confirmation = Terminal::ReadLine("Confirmar destruicao? (s/n): ");
        if (confirmation.empty() || std::tolower(static_cast<unsigned char>(confirmation[0])) != 's')
        {
            return;
        }

        orchestrator->Destroy(environment);
        store.Save(environments);

        std::cout << "\nAmbiente destruido com sucesso.\n";
        Terminal::Pause();
    }

    void ShowTemplates() const
    {
        Terminal::Header("Templates de infraestrutura");

        for (const EnvironmentTemplate& selectedTemplate : templates)
        {
            std::cout << selectedTemplate.name << "\n";
            std::cout << "  ID: " << selectedTemplate.id << "\n";
            std::cout << "  Uso: " << selectedTemplate.description << "\n";
            std::cout << "  Custo: US$ " << std::fixed << std::setprecision(2) << selectedTemplate.hourlyCostUsd << "/h\n";
            std::cout << "  Permissao: " << (selectedTemplate.requiresAdmin ? "Admin" : "Developer+") << "\n\n";
        }

        Terminal::Pause();
    }

    int SelectEnvironment(const std::string& label) const
    {
        if (environments.empty())
        {
            std::cout << "Nenhum ambiente encontrado.\n";
            Terminal::Pause();
            return 0;
        }

        for (std::size_t index = 0; index < environments.size(); ++index)
        {
            const Environment& environment = environments[index];
            std::cout << index + 1 << ". " << environment.id
                      << " | " << EnvironmentStore::StatusName(environment.status)
                      << " | " << environment.repository
                      << " | " << environment.branch << "\n";
        }
        std::cout << "0. Voltar\n\n";

        return Terminal::ReadOption(label, 0, static_cast<int>(environments.size()));
    }

    void ExpireOldEnvironments()
    {
        const std::time_t now = Clock::to_time_t(Clock::now());

        for (Environment& environment : environments)
        {
            if ((environment.status == EnvironmentStatus::Running || environment.status == EnvironmentStatus::Creating) && environment.expiresAt <= now)
            {
                environment.status = EnvironmentStatus::Expired;
                environment.logs.push_back("TTL expirado. Ambiente marcado para limpeza automatica.");
            }
        }
    }

    int CountActiveEnvironments() const
    {
        return static_cast<int>(std::count_if(environments.begin(), environments.end(), [](const Environment& environment)
        {
            return environment.status == EnvironmentStatus::Running || environment.status == EnvironmentStatus::Creating;
        }));
    }

    double CurrentHourlyCost() const
    {
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

    static std::string RoleName(UserRole role)
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
};

int main()
{
    OrbitDesktopApp app;
    app.Run();
    return 0;
}
