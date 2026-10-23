#pragma once
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

enum class MakerState { Idle, Running, Done, Failed };

struct MakerResult {
    MakerState state = MakerState::Idle;
    std::filesystem::path song;
    std::string message;
    std::string lrc;
    bool matches = false;
    float words = 0.0f;
    float coverage = 0.0f;
    int lines = 0;
    int weakLines = 0;
    std::vector<std::string> dropped;
};

class LyricsMaker {
    public:
        LyricsMaker() = default;
        ~LyricsMaker();
        LyricsMaker(const LyricsMaker&) = delete;
        LyricsMaker& operator=(const LyricsMaker&) = delete;

        static bool available();
        bool start(const std::filesystem::path& song, const std::string& lyrics);
        MakerResult snapshot();

    private:
        void worker(std::filesystem::path song);
        static std::filesystem::path workFolder();

        std::mutex mutex;
        MakerResult shared;
        bool stopping = false;
        std::thread thread;
};
