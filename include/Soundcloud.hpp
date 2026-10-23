#pragma once
#include <string>
#include <vector>
#include <mutex>
#include <thread>
#include <filesystem>

enum class DownloadState{ Downloading, Done, Failed};

struct RemoteTrack {
    std::string id;
    std::string title;
    std::string uploader;
    std::string url;
    float duration{0.0f};
};

struct Download {
    RemoteTrack track;
    DownloadState state = DownloadState::Downloading;
};

enum class SearchState { Idle, Searching, Results, Error};

struct SearchSnapshot {
    SearchState state = SearchState::Idle;
    std::vector<RemoteTrack> results;
    std::string error;
    std::vector<Download> downloads;
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
        static bool runDownload(const std::string& url, const std::filesystem::path& folder, std::string& error);
        void startDownload(const RemoteTrack& track);
        void setDownloadFolder(const std::filesystem::path& folder);
        bool takeFinishedDownloads();

    private:
        void searchWorker(std::string query);
        std::mutex mutex;
        SearchSnapshot shared;
        std::thread searchThread;
        void downloadWorker(RemoteTrack track, std::filesystem::path folder);
        std::vector<std::thread> downloadThreads;
        std::filesystem::path downloadFolder;
        bool downloadFinished = false;

    };