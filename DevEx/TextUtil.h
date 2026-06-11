#pragma once

#include <string>
#include <vector>

class TextUtil
{
public:
    static std::string ToLower(std::string value);
    static std::string Slug(const std::string& value);
    static std::string Join(const std::vector<std::string>& values, char delimiter);
    static std::vector<std::string> Split(const std::string& value, char delimiter);
    static std::string Trim(const std::string& str);
};
