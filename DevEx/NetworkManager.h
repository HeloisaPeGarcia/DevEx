#pragma once

#include <string>

class NetworkManager
{
public:
    static void UpdateHostsEntry(const std::string& hostname, const std::string& ip);
};
