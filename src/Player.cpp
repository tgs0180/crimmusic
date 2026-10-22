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

bool Player::isFinished() {
    return soundLoaded && ma_sound_at_end(&sound);
}

bool Player::isPlaying() {
    return soundLoaded && ma_sound_is_playing(&sound);
}

void Player::setVolume(float volume) {
    if (!engineReady) return;
    ma_engine_set_volume(&engine, volume);
}

float Player::volume() {
    if (!engineReady) return 0.0f;
    return ma_engine_get_volume(&engine);
}

float Player::position() {
    if (!soundLoaded) return 0.0f;
    float seconds = 0.0f;
    ma_sound_get_cursor_in_seconds(&sound, &seconds);
    return seconds;
}

float Player::length() {
    if (!soundLoaded) return 0.0f;
    float seconds = 0.0f;
    ma_sound_get_length_in_seconds(&sound, &seconds);
    return seconds;
}

void Player::seek(float seconds) {
    if (!soundLoaded) return;
    ma_sound_seek_to_second(&sound, seconds);
}

void Player::togglePause() {
    if (!soundLoaded) return;
    bool isPlaying = ma_sound_is_playing(&sound);
    if (isPlaying) ma_sound_stop(&sound);
    else ma_sound_start(&sound);

}

void Player::stop() {
    if (soundLoaded) ma_sound_uninit(&sound);
    soundLoaded = false;
}

Player::~Player() {
    if (soundLoaded) ma_sound_uninit(&sound);
    if (engineReady) ma_engine_uninit(&engine);
}