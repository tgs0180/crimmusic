#pragma once
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <future>
#include <string>

class Player;
class Library;

class Discord {
public:
    Discord() = default;
    ~Discord();
    Discord(const Discord&) = delete;
    Discord& operator=(const Discord&) = delete;

    void poll(Player& player, Library& library);

private:
    bool connectToDiscord();
    void disconnect();
    bool sendFrame(std::uint32_t opcode, const std::string& json);
    void drain();
    void sendActivity(Player& player, Library& library, bool playing);

    int sock = -1;
    int nonce = 0;
    std::chrono::steady_clock::time_point nextConnectTry;
    std::chrono::steady_clock::time_point sentAt;
    std::filesystem::path sentSong;
    bool sentPlaying = false;
    float sentPosition = 0.0f;
    std::future<std::string> coverLookup;
    std::filesystem::path coverSong;
    std::string coverUrl;
    bool coverChanged = false;
};
