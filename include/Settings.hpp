#pragma once
#include <filesystem>

class Settings {
public:
    std::filesystem::path musicFolder;
    float volume = 1.0f;

    void load();
    void save() const;

private:
    std::filesystem::path filePath() const;
};
