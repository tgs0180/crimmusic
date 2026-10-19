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
    ma_sound sound;
    ma_result result = ma_sound_init_from_file(&engine, songs[0].string().c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed loading a song" << "\n";
        ma_engine_uninit(&engine);
        return 1;
    }
    ma_sound_start(&sound);
    std::cout << "Playing: " << songs[0].string() << "\n";
    std::string command;
    
    
    
    while (true) {
        std::cout << "> ";
        std::getline(std::cin, command);

        if (command == "q") {
            break;
        }
        else if (command == "p") {
            bool isPlaying = ma_sound_is_playing(&sound);
            if (isPlaying) ma_sound_stop(&sound);
            else if (!isPlaying) ma_sound_start(&sound);
        }
        
        
        
        else if (!std::getline(std::cin, command)) break;

        else std::cout << "Command not found" << "\n";
    }




    ma_sound_uninit(&sound);
    ma_engine_uninit(&engine);
    return 0;
}

