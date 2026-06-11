#pragma once

#include <string>
#include <vector>
#include <ctime>

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
    std::string token;
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
