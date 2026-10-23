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
#include "Lyrics.hpp"
#include "LyricsMaker.hpp"

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
        void drawLyrics(Player& player, Library& library, float x, float y, float width, float height);

        Lyrics lyrics;
        std::filesystem::path lyricsSong;
        int shownLine = -1;
        int previousLine = -1;
        float lineSlide = 1.0f;

        void drawMakeLyrics(Library& library);
        void saveMadeLyrics(const MakerResult& result);
        static void onMp3Chosen(void* userdata, const char* const* filelist, int filter);
        static void onTxtChosen(void* userdata, const char* const* filelist, int filter);
        LyricsMaker maker;
        bool showMakeLyrics{false};
        std::filesystem::path makeSong;
        char makeLyricsText[65536] = "";
        std::string pendingMp3;
        std::string pendingTxt;
        std::string dialogLocation;
        std::string makeStatus;

        struct EditRow {
            char time[16] = "";
            char text[512] = "";
        };
        void loadEditLyrics(const std::filesystem::path& song);
        bool saveEditLyrics();
        void drawEditLyrics(Player& player, Library& library);
        bool showEditLyrics{false};
        std::filesystem::path editSong;
        std::vector<std::string> editHeader;
        std::vector<EditRow> editRows;
        bool editDirty{false};
        std::string editStatus;
};