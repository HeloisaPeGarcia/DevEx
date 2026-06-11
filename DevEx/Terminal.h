#pragma once

#include <string>
#include <ctime>

class Terminal
{
public:
    static void Clear();
    static void Pause();
    static void Header(const std::string& subtitle);
    static int ReadOption(const std::string& label, int minimum, int maximum);
    static std::string ReadLine(const std::string& label);
    static std::string FormatTime(std::time_t value);
    static std::string ProgressBar(int progress);
};
