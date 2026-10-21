#include "Settings.hpp"
#include <cstdlib>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <fstream>
#include <string>

std::filesystem::path Settings::filePath() const {
    const char* env = std::getenv("HOME");
    if (env == nullptr) return {};
    return std::filesystem::path(env) / ".config" / "crimmusic" / "settings.txt";
}

void Settings::save() const {
    std::filesystem::path path = filePath();
    if (path.empty()) return;
    std::filesystem::create_directories(path.parent_path());
    std::ofstream file(path);
    file << "folder=" << musicFolder.string() << "\n";
    file << "volume=" << volume << "\n";
}

void Settings::load() {
    std::filesystem::path path = filePath();
    if (path.empty()) return;
    std::ifstream file(path);
    if (!file) return;
    std::string line{};
    while (std::getline(file, line)) {
        std::size_t pos = line.find("=");
        if (pos == std::string::npos) continue;
        std::string key = line.substr(0, pos);
        std::string value = line.substr(pos+1);
        if (key == "folder") {
            musicFolder = value;
        }
        else if (key == "volume") {
            try {
                volume = std::stof(value);
            }
            catch (const std::exception&) {
                // bad value in the file, keep the default
            }
        }
    }
}
