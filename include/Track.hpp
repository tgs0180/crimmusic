#pragma once
#include <filesystem>
#include <string>

struct Track {
    std::filesystem::path file;
    std::string title;
    std::string artist;
};