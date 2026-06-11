#include "NetworkManager.h"
#include <fstream>
#include <iostream>

void NetworkManager::UpdateHostsEntry(const std::string& hostname, const std::string& ip)
{
#ifdef _WIN32
    std::ofstream file("C:\\Windows\\System32\\drivers\\etc\\hosts", std::ios::app);
    if (file)
    {
        file << "\n" << ip << "\t" << hostname << "\t# Added by OrbitDesktop\n";
        std::cout << "[NetworkManager] Added entry: " << ip << " " << hostname << "\n";
    }
    else
    {
        std::cout << "[NetworkManager] Permission note: Run as Administrator to write DNS mapping to hosts file.\n";
    }
#else
    std::ofstream file("/etc/hosts", std::ios::app);
    if (file)
    {
        file << "\n" << ip << "\t" << hostname << "\t# Added by OrbitDesktop\n";
    }
#endif
}
