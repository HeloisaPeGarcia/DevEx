#include "OrbitDesktopApp.h"
#include <iostream>
#include <string>

void PrintHelp()
{
    std::cout << "OrbitDesktop CLI - Headless Preview Environment Orchestrator\n\n"
              << "Usage:\n"
              << "  DevEx.exe                     Start interactive console UI mode\n"
              << "  DevEx.exe --help | -h          Show help instructions\n"
              << "  DevEx.exe --list              List all active preview environments in JSON format\n"
              << "  DevEx.exe --daemon            Start background daemon to monitor and teardown expired environments\n"
              << "  DevEx.exe --launch --repo <repo> --branch <branch> --template <template> [--ttl <hours>]\n"
              << "                                Provision a new preview environment\n"
              << "  DevEx.exe --nuke --id <id>    Destroy an existing environment\n\n"
              << "Options:\n"
              << "  --repo        Repository owner/name (e.g., octocat-dev/sales-tracking-system)\n"
              << "  --branch      Git branch name (e.g., feature/new-checkout)\n"
              << "  --template    Infrastructure template ID (e.g., web-postgres)\n"
              << "  --ttl         Time-to-Live in hours (defaults to 2 if not provided)\n"
              << "  --id          The environment ID to teardown\n\n"
              << "Environment Variables:\n"
              << "  ORBIT_TOKEN         The Git provider token (required for headless operations)\n"
              << "  ORBIT_STORE_PATH    Path to the database file (defaults to orbitdesktop.environments.tsv)\n";
}

int main(int argc, char* argv[])
{
    OrbitDesktopApp app;

    if (argc == 1)
    {
        app.Run();
        return 0;
    }

    std::string arg1 = argv[1];

    if (arg1 == "--help" || arg1 == "-h")
    {
        PrintHelp();
        return 0;
    }
    else if (arg1 == "--list")
    {
        app.RunHeadlessList();
        return 0;
    }
    else if (arg1 == "--daemon")
    {
        app.RunDaemon();
        return 0;
    }
    else if (arg1 == "--nuke")
    {
        std::string envId;
        for (int i = 2; i < argc; ++i)
        {
            std::string opt = argv[i];
            if (opt == "--id" && i + 1 < argc)
            {
                envId = argv[i + 1];
                break;
            }
        }
        if (envId.empty())
        {
            std::cerr << "{\"error\": \"Missing required option --id\"}\n";
            return 1;
        }
        return app.RunHeadlessNuke(envId) ? 0 : 1;
    }
    else if (arg1 == "--launch")
    {
        std::string repo, branch, templateId;
        int ttlHours = 2;

        for (int i = 2; i < argc; ++i)
        {
            std::string opt = argv[i];
            if (opt == "--repo" && i + 1 < argc)
            {
                repo = argv[i + 1];
            }
            else if (opt == "--branch" && i + 1 < argc)
            {
                branch = argv[i + 1];
            }
            else if (opt == "--template" && i + 1 < argc)
            {
                templateId = argv[i + 1];
            }
            else if (opt == "--ttl" && i + 1 < argc)
            {
                try {
                    ttlHours = std::stoi(argv[i + 1]);
                } catch(...) {}
            }
        }

        if (repo.empty() || branch.empty() || templateId.empty())
        {
            std::cerr << "{\"error\": \"Missing required options for launch. Check --help.\"}\n";
            return 1;
        }

        return app.RunHeadlessLaunch(repo, branch, templateId, ttlHours) ? 0 : 1;
    }
    else
    {
        std::cerr << "{\"error\": \"Unknown command line argument: " << arg1 << ". Run with --help.\"}\n";
        return 1;
    }
}
