#include "Player.hpp"
#include <iostream>


bool Player::init() {
    if (ma_engine_init(NULL, &engine) != MA_SUCCESS) {
        std::cerr << "ERROR: failed to start audio engine" << "\n";
        return false;
    }
    engineReady = true;
    return true;
}

bool Player::play(const std::filesystem::path &file) {
    if (soundLoaded) {
        ma_sound_uninit(&sound);
        soundLoaded = false;
    }
    ma_result result = ma_sound_init_from_file(&engine, file.string().c_str(), MA_SOUND_FLAG_STREAM, NULL, NULL, &sound);
    if (result != MA_SUCCESS) {
        std::cerr << "Failed loading a song" << "\n";    
        return false;
    }
    soundLoaded = true;
    ma_sound_start(&sound);
    return true;
}

void Player::togglePause() {
    if (!soundLoaded) return;
    bool isPlaying = ma_sound_is_playing(&sound);
    if (isPlaying) ma_sound_stop(&sound);
    else ma_sound_start(&sound);

}

Player::~Player() {
    if (soundLoaded) ma_sound_uninit(&sound);
    if (engineReady) ma_engine_uninit(&engine);
}