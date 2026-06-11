#include "TextUtil.h"
#include <algorithm>
#include <cctype>
#include <sstream>

std::string TextUtil::ToLower(std::string value)
{
    for (char& character : value)
    {
        character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

std::string TextUtil::Slug(const std::string& value)
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

std::string TextUtil::Join(const std::vector<std::string>& values, char delimiter)
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

std::vector<std::string> TextUtil::Split(const std::string& value, char delimiter)
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

std::string TextUtil::Trim(const std::string& str)
{
    const std::size_t first = str.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
    {
        return "";
    }
    const std::size_t last = str.find_last_not_of(" \t\r\n");
    return str.substr(first, last - first + 1);
}
