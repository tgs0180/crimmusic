#pragma once
#include <string>
#include <algorithm>
#include <cctype>

inline std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c);});
    return s;
}
