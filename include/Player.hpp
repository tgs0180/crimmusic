#pragma once
#include <filesystem>
#include "miniaudio.h"

class Player {
    public: 
        Player() = default;
        Player(const Player&) = delete;
        Player& operator=(const Player&) = delete;

        bool init();
        bool play(const std::filesystem::path&file);
        void togglePause();
        bool isFinished();
        bool isPlaying();
        void setVolume(float volume);
        float volume();
        float position();
        float length();
        void seek(float seconds);
        ~Player();

    private:
        ma_engine engine;
        ma_sound sound;
        bool engineReady = false;
        bool soundLoaded = false;
};