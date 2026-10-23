#pragma once
class Player;
class Library;
class SoundCloud;
struct SDL_Window;
struct SDL_Renderer;
#include <string>
#include <mutex>
#include <filesystem>
#include "CoverCache.hpp"
#include "IconSet.hpp"

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
        bool NowPlayingTarget{false};
        float nowPlayingSlide{0.0f};
        bool JustOpened{false};
        
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
        bool iconButton(const char* iconName, bool active = false, float size = 0.0f);

        std::mutex folderMutex;
        std::string chosenFolder;

        CoverCache covers{160};
        CoverCache bigCovers{512};
        IconSet icons;
        void drawPlayPause(Player& player, float iconSize = 0.0f);
        void drawRepeat(Library& lib, float iconSize = 0.0f);
        void drawShuffle(Library& lib, float iconSize = 0.0f);
        void drawQueue(Player& player, Library& lib, float iconSize = 0.0f);
        void drawVolume(Player& player, float width);
        void drawTimeSlider(Player& player, float width);

};