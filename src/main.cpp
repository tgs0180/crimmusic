#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include "miniaudio.h"

namespace fs = std::filesystem;


std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c);});
    return s;
}

std::vector<fs::path> findMp3s(const fs::path& root)
{
    std::vector<fs::path> songs;

    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::string extResult = toLower(extension);
        if (extResult != ".mp3") continue;
        songs.push_back(entry.path());
        
    }

    std::sort(songs.begin(), songs.end());
    return songs;
}

int main(int argc, char* argv[]){
    if (argc < 2) {
        std::cerr << "Usage: crimmusic <music folder>"<< "\n";
        return 1;
    }

    fs::path root = argv[1];

    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Not a folder: " << root.string() << "\n";
        return 1;

    }
    std::vector<fs::path> songs = findMp3s(root);
    std::cout << "Found " << songs.size() << " songs" << "\n";
    for (std::size_t i = 0; i < songs.size(); i++) {
        std::cout << i+1 << ". " << songs[i].string() << "\n";
    }
    if (songs.empty()) {
        std::cerr << "ERROR: No songs found" << "\n";
        return 1;
    }
    ma_engine engine;
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        std::cerr << "ERROR: failed to start audio engine" << "\n";
        return 1;
    }
    ma_engine_play_sound(&engine, songs[0].string().c_str(), NULL);
    std::cout << "Playing: " << songs[0].string() << " Press Enter to quit." << "\n";
    std::cin.get();
    ma_engine_uninit(&engine);
    return 0;
}

