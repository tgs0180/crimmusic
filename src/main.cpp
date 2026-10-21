#include <iostream>
#include <filesystem>
#include "Player.hpp"
#include "UI.hpp"
#include "Library.hpp"

namespace fs = std::filesystem;

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
    Library library;
    if (!library.scan(root)) {
        std::cerr << "ERROR: No songs found" << "\n";
        return 1;
    }
    std::cout << "Found " << library.size() << " songs" << "\n";
    Player player;
    if (!player.init()) return 1;
    
    if (!player.play(library.currentSong())) return 1;

    UI ui;
    if (!ui.init()) return 1;

    while (ui.processEvents()) {
        if (player.isFinished()) {
            library.next();
            player.play(library.currentSong());
        }
        ui.draw(player, library);
    }
    return 0;
}

