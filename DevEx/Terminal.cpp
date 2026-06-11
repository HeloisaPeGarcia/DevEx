#include "Terminal.h"
#include <iostream>
#include <limits>
#include <iomanip>
#include <sstream>

void Terminal::Clear()
{
    std::cout << "\x1B[2J\x1B[H";
}

void Terminal::Pause()
{
    std::cout << "\nPress Enter to continue...";
    std::cin.ignore((std::numeric_limits<std::streamsize>::max)(), '\n');
}

void Terminal::Header(const std::string& subtitle)
{
    Clear();
    std::cout << "============================================================\n";
    std::cout << " OrbitDesktop\n";
    std::cout << " Ephemeral environments via GitHub Actions, Terraform & K8s\n";
    std::cout << "============================================================\n";
    std::cout << subtitle << "\n\n";
}

int Terminal::ReadOption(const std::string& label, int minimum, int maximum)
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
        std::cout << "Invalid option. Please try again.\n";
    }
}

std::string Terminal::ReadLine(const std::string& label)
{
    std::cout << label;
    std::string value;
    std::getline(std::cin, value);
    return value;
}

std::string Terminal::FormatTime(std::time_t value)
{
    if (value <= 0)
    {
        return "-";
    }

    std::tm localTime{};
    localtime_s(&localTime, &value);

    std::ostringstream output;
    output << std::put_time(&localTime, "%Y-%m-%d %H:%M");
    return output.str();
}

std::string Terminal::ProgressBar(int progress)
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
