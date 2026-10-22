#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>


struct RemoteTrack {
    std::string id;
    std::string title;
    std::string uploader;
    std::string url;
    float duration{0.0f};
};

enum class SearchState { Idle, Searching, Results, Error};

struct SearchSnapshot {
    SearchState state = SearchState::Idle;
    std::vector<RemoteTrack> results;
    std::string error;
};

class SoundCloud {
    public:
        static bool runSearch(const std::string& query, std::vector<RemoteTrack>& out, std::string& error);
        SoundCloud() = default;
        ~SoundCloud();
        SoundCloud(const SoundCloud&) = delete;
        SoundCloud& operator=(const SoundCloud&) = delete;
        void startSearch(const std::string& query);
        SearchSnapshot snapshot();

    private:
        void searchWorker(std::string query);
        std::mutex mutex;
        SearchSnapshot shared;
        std::thread searchThread;

    };