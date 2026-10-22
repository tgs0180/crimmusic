#include <iostream>
#include <filesystem>
#include "Player.hpp"
#include "Soundcloud.hpp"
#include "UI.hpp"
#include "Library.hpp"
#include "Settings.hpp"
#include "Mpris.hpp"

namespace fs = std::filesystem;

int main(int argc, char* argv[]){
    Settings settings; settings.load();
    fs::path root = "";
    if (argc > 1) root = argv[1];
    else root = settings.musicFolder;

    SoundCloud soundcloud;
    Library library;
    Player player;
    if (!player.init()) return 1;
    player.setVolume(settings.volume);
    if (!root.empty() && library.scan(root)){
        std::cout << "Found " << library.size() << " songs" << "\n";
        if (!player.play(library.currentSong())) return 1;
    }
    soundcloud.setDownloadFolder(root);

    Mpris mpris;
    if (!mpris.init(player, library)) std::cerr << "MPRIS not available, media keys wont work\n";

    UI ui;
    if (!ui.init()) return 1;

    while (ui.processEvents()) {
        mpris.poll();
        if (player.isFinished()) {
            if (library.repeatMode() == RepeatMode::One) player.play(library.currentSong());
            else if (library.repeatMode() == RepeatMode::Off) {
                if (library.atEnd()) player.stop();
                else {library.next(); player.play(library.currentSong());}
            }
            else if (library.repeatMode() == RepeatMode::All) {
                library.next(); player.play(library.currentSong());
            }
        }
        
        fs::path newFolder;
        if (ui.takeChosenFolder(newFolder)) {
            if(library.scan(newFolder)) {
                player.play(library.currentSong());
                settings.musicFolder = newFolder;
                soundcloud.setDownloadFolder(newFolder);
            }
        }

        if (soundcloud.takeFinishedDownloads()) {
            library.rescan();
        }
        ui.draw(player, library, soundcloud);
    }
    settings.volume = player.volume();
    settings.save();
    return 0;
}

