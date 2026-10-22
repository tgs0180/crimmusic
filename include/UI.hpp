#pragma once
class Player;
class Library;
class SoundCloud;
struct SDL_Window;
struct SDL_Renderer;
#include <string>
#include <mutex>
#include <filesystem>

class UI {
    public:
        UI() =default;
        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;
        ~UI();

        bool init();
        bool processEvents();
        void draw(Player& player, Library& library, SoundCloud& soundcloud);
        bool takeChosenFolder(std::filesystem::path& out);
        
    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool sdlReady = false;
        bool imguiReady = false;
        char searchText[128] = "";
        bool quit{false};
        static void onFolderChosen(void* userdata, const char* const* filelist, int filter);
        bool showSoundCloud{false};
        char soundCloudQuery[128] = "";

        std::mutex folderMutex;
        std::string chosenFolder;

};