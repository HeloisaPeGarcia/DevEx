#include "Orchestrator.h"
#include "TextUtil.h"
#include "Terminal.h"
#include <chrono>
#include <thread>

using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

Environment SimulatedEnvironmentOrchestrator::Launch(const UserSession& session, const Repository& repository, const std::string& branch, const EnvironmentTemplate& selectedTemplate, int ttlHours, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const
{
    const std::string repoSlug = TextUtil::Slug(repository.name);
    const std::string branchSlug = TextUtil::Slug(branch);
    const std::string templateSlug = TextUtil::Slug(selectedTemplate.name);
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

    const std::string initialLine = "[" + Terminal::ProgressBar(0) + "] 0%  Initializing provisioning sequence...";
    environment.logs.push_back(initialLine);

    std::string envId = environment.id;
    std::string repo = environment.repository;
    std::string appUrl = environment.appUrl;

    std::thread([envId, repo, branch, appUrl, onUpdate]() {
        const std::vector<std::string> steps = {
            "Validating token permissions and scopes",
            "POST /repos/" + repo + "/dispatches",
            "repository_dispatch event received by GitHub Actions",
            "Clonando branch " + branch,
            "Building Docker image",
            "Running terraform plan",
            "Running terraform apply",
            "Creating isolated Kubernetes namespace",
            "Provisioning temporary database",
            "Applying Kubernetes manifests via kubectl apply",
            "Waiting for deployment health checks",
            "Publishing temporary application URL and credentials"
        };

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const int progress = static_cast<int>(((index + 1) * 100) / steps.size());
            const std::string line = "[" + Terminal::ProgressBar(progress) + "] " + std::to_string(progress) + "%  " + steps[index] + "...";
            onUpdate(envId, EnvironmentStatus::Creating, line);
            std::this_thread::sleep_for(250ms);
        }

        onUpdate(envId, EnvironmentStatus::Running, "Environment online: " + appUrl);
    }).detach();

    return environment;
}

void SimulatedEnvironmentOrchestrator::Destroy(Environment& environment, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const
{
    std::string envId = environment.id;

    std::thread([envId, onUpdate]() {
        const std::vector<std::string> steps = {
            "Dispatching orbitdesktop.destroy event",
            "Removing Kubernetes ingress and services",
            "Terminating pods in isolated namespace",
            "Teardown temporary database",
            "Running terraform destroy",
            "Deallocating cloud resources"
        };

        for (std::size_t index = 0; index < steps.size(); ++index)
        {
            const int progress = static_cast<int>(((index + 1) * 100) / steps.size());
            const std::string line = "[" + Terminal::ProgressBar(progress) + "] " + std::to_string(progress) + "%  " + steps[index] + "...";
            onUpdate(envId, EnvironmentStatus::Destroying, line);
            std::this_thread::sleep_for(250ms);
        }

        onUpdate(envId, EnvironmentStatus::Destroyed, "Environment successfully destroyed.");
    }).detach();
}
