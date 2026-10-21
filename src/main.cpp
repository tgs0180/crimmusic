#include <iostream>
#include <filesystem>
#include "Player.hpp"
#include "UI.hpp"
#include "Library.hpp"
#include "Settings.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]){
    Settings settings; settings.load();
    fs::path root = "";
    if (argc > 1) root = argv[1];
    else root = settings.musicFolder;

    Library library;
    Player player;
    if (!player.init()) return 1;
    player.setVolume(settings.volume);
    if (!root.empty() && library.scan(root)){
        std::cout << "Found " << library.size() << " songs" << "\n";
        if (!player.play(library.currentSong())) return 1;
    }

    UI ui;
    if (!ui.init()) return 1;

    while (ui.processEvents()) {
        if (player.isFinished()) {
            library.next();
            player.play(library.currentSong());
        }
        
        fs::path newFolder;
        if (ui.takeChosenFolder(newFolder)) {
            if(library.scan(newFolder)) {
                player.play(library.currentSong());
                settings.musicFolder = newFolder;
            }
        
        }
        ui.draw(player, library);
    }
    settings.volume = player.volume();
    settings.save();
    return 0;
}

