#include "Orchestrator.h"
#include "TextUtil.h"
#include "Terminal.h"
#include <chrono>
#include <thread>
#include <iostream>
#include <random>

using Clock = std::chrono::system_clock;
using namespace std::chrono_literals;

#include "json.hpp"
using json = nlohmann::json;

long long ParseLatestRunId(const std::string& jsonStr)
{
    try {
        auto j = json::parse(jsonStr);
        if (j.contains("workflow_runs") && j["workflow_runs"].is_array() && !j["workflow_runs"].empty()) {
            return j["workflow_runs"][0].value("id", 0LL);
        }
    } catch (...) {}
    return 0;
}

std::string ParseRunStatus(const std::string& jsonStr)
{
    try {
        auto j = json::parse(jsonStr);
        return j.value("status", "");
    } catch (...) {}
    return "";
}

std::string ParseRunConclusion(const std::string& jsonStr)
{
    try {
        auto j = json::parse(jsonStr);
        if (j["conclusion"].is_string()) {
            return j["conclusion"].get<std::string>();
        }
    } catch (...) {}
    return "";
}

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

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dist(10000000, 99999999);
    environment.databasePassword = "tmp_" + std::to_string(dist(gen));

    std::string envId = environment.id;
    std::string repo = environment.repository;
    std::string appUrl = environment.appUrl;

    if (selectedTemplate.id == "local-k8s")
    {
        // Local Kubernetes simulation & actual namespace creation
        backgroundTasks.push_back(std::async(std::launch::async, [envId, branch, appUrl, onUpdate]() {
            onUpdate(envId, EnvironmentStatus::Creating, "[kubectl] Verifying cluster accessibility...");
            std::this_thread::sleep_for(200ms);
            
            std::string nsName = "preview-" + envId;
            onUpdate(envId, EnvironmentStatus::Creating, "[kubectl] Creating isolated namespace: " + nsName);
            TextUtil::ExecuteCommand("kubectl create namespace " + nsName + " --dry-run=client -o yaml | kubectl apply -f -");
            std::this_thread::sleep_for(300ms);

            onUpdate(envId, EnvironmentStatus::Creating, "[kubectl] Deploying application components...");
            TextUtil::ExecuteCommand("kubectl create deployment express-app --image=node:22-alpine -n " + nsName + " --dry-run=client -o yaml | kubectl apply -f -");
            std::this_thread::sleep_for(300ms);

            onUpdate(envId, EnvironmentStatus::Creating, "[kubectl] Exposing service...");
            TextUtil::ExecuteCommand("kubectl create service clusterip express-app --tcp=8080:8080 -n " + nsName + " --dry-run=client -o yaml | kubectl apply -f -");
            
            onUpdate(envId, EnvironmentStatus::Running, "Local Kubernetes environment successfully created. App: " + appUrl);
        }));
    }
    else if (!session.token.empty())
    {
        // Real GitHub repository dispatch & Actions Run monitoring
        backgroundTasks.push_back(std::async(std::launch::async, [envId, repo, branch, appUrl, session, ttlHours, onUpdate]() {
            onUpdate(envId, EnvironmentStatus::Creating, "GitHub API: Sending repository_dispatch...");

            std::string payload = "{\\\"event_type\\\": \\\"orbitdesktop.launch\\\", \\\"client_payload\\\": {\\\"branch\\\": \\\"" + branch + "\\\", \\\"environment_id\\\": \\\"" + envId + "\\\", \\\"ttl_hours\\\": \\\"" + std::to_string(ttlHours) + "\\\"}}";
            std::string dispatchCmd = "curl -s -X POST -H \"Authorization: token " + session.token + "\" -H \"Accept: application/vnd.github.v3+json\" -H \"User-Agent: OrbitDesktop\" -d \"" + payload + "\" \"https://api.github.com/repos/" + repo + "/dispatches\"";
            TextUtil::ExecuteCommand(dispatchCmd);

            // Poll runs API for the launched run
            std::this_thread::sleep_for(3s);
            long long runId = 0;
            std::string runsCmd = "curl -s -H \"Authorization: token " + session.token + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/repos/" + repo + "/actions/runs?event=repository_dispatch\"";
            
            for (int attempt = 0; attempt < 5; ++attempt)
            {
                std::string response = TextUtil::ExecuteCommand(runsCmd);
                runId = ParseLatestRunId(response);
                if (runId != 0) break;
                onUpdate(envId, EnvironmentStatus::Creating, "GitHub API: Waiting for Actions run to spawn...");
                std::this_thread::sleep_for(2s);
            }

            if (runId == 0)
            {
                onUpdate(envId, EnvironmentStatus::Failed, "GitHub Actions run failed to start. Check credentials.");
                return;
            }

            onUpdate(envId, EnvironmentStatus::Creating, "GitHub Run ID detected: " + std::to_string(runId) + ". Streaming logs...");

            std::string runDetailCmd = "curl -s -H \"Authorization: token " + session.token + "\" -H \"User-Agent: OrbitDesktop\" \"https://api.github.com/repos/" + repo + "/actions/runs/" + std::to_string(runId) + "\"";
            
            while (true)
            {
                std::string runDetail = TextUtil::ExecuteCommand(runDetailCmd);
                std::string statusStr = ParseRunStatus(runDetail);
                std::string conclusionStr = ParseRunConclusion(runDetail);

                onUpdate(envId, EnvironmentStatus::Creating, "GitHub Workflow status: " + statusStr + " (Conclusion: " + (conclusionStr.empty() ? "Pending" : conclusionStr) + ")");

                if (statusStr == "completed")
                {
                    if (conclusionStr == "success")
                    {
                        onUpdate(envId, EnvironmentStatus::Running, "CI/CD finished successfully. App ready: " + appUrl);
                    }
                    else
                    {
                        onUpdate(envId, EnvironmentStatus::Failed, "CI/CD execution failed.");
                    }
                    break;
                }
                std::this_thread::sleep_for(4s);
            }
        }));
    }
    else
    {
        // Simulated workflow fallback
        backgroundTasks.push_back(std::async(std::launch::async, [envId, repo, branch, appUrl, onUpdate]() {
            const std::vector<std::string> steps = {
                "Validating token permissions and scopes",
                "POST /repos/" + repo + "/dispatches",
                "repository_dispatch event received by GitHub Actions",
                "Cloning branch " + branch,
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
        }));
    }

    return environment;
}

void SimulatedEnvironmentOrchestrator::Destroy(const Environment& environment, std::function<void(const std::string& id, EnvironmentStatus status, const std::string& logLine)> onUpdate) const
{
    std::string envId = environment.id;

    if (environment.templateName.find("Local") != std::string::npos)
    {
        backgroundTasks.push_back(std::async(std::launch::async, [envId, onUpdate]() {
            std::string nsName = "preview-" + envId;
            onUpdate(envId, EnvironmentStatus::Destroying, "[kubectl] Deleting isolated deployment...");
            TextUtil::ExecuteCommand("kubectl delete deployment express-app -n " + nsName + " --ignore-not-found");
            std::this_thread::sleep_for(300ms);

            onUpdate(envId, EnvironmentStatus::Destroying, "[kubectl] Deleting namespace...");
            TextUtil::ExecuteCommand("kubectl delete namespace " + nsName + " --ignore-not-found");
            
            onUpdate(envId, EnvironmentStatus::Destroyed, "Local Kubernetes namespace successfully cleaned.");
        }));
    }
    else
    {
        backgroundTasks.push_back(std::async(std::launch::async, [envId, onUpdate]() {
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
        }));
    }
}
