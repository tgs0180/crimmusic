#include "Soundcloud.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_process.h>
#include <cstddef>
#include <exception>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>


bool SoundCloud::runSearch(const std::string& query, Source source, std::vector<RemoteTrack>& out, std::string& error) {
    std::string prefix = source == Source::YouTube ? "ytsearch10:" : "scsearch5:";
    std::string searchArg = prefix + query;
    const char* args[] = { "yt-dlp", "--flat-playlist", "--dump-json", "--no-warnings", searchArg.c_str(), nullptr};
    SDL_Process* process = SDL_CreateProcess(args, true);
    if (process == nullptr) {
        error = "Could not start yt-dlp";
        return false;
    }
    size_t size = 0;
    int exitCode = -1;
    void* data = SDL_ReadProcess(process, &size, &exitCode);
    std::string output;
    if (data != nullptr) {
        output.assign(static_cast<char*>(data), size);
        SDL_free(data);
    }
    SDL_DestroyProcess(process);
    if (exitCode != 0) {error = "yt-dlp failed"; return false;}
    auto streamoutput = std::istringstream(output);
    std::string line;
    while (std::getline(streamoutput, line)){
        if (line.empty()) continue;
        try {
            auto json = nlohmann::json::parse(line);
            RemoteTrack track;
            track.title = json.value("title", "Unknown");
            track.uploader = json.value("uploader", "Unknown");
            track.url = json.value("webpage_url", "");
            track.id = json.value("id", "");
            if (json.contains("duration") && json["duration"].is_number()) track.duration = json["duration"].get<float>();
            track.source = source;
            out.push_back(track);
        }
        catch (const std::exception&) {
            continue;
        }
    }
    return true;
}

SearchSnapshot SoundCloud::snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return shared;
}

void SoundCloud::searchWorker(std::string query, Source source) {
    std::vector<RemoteTrack> localResults;
    std::string error;
    bool ok = runSearch(query, source, localResults, error);
    std::lock_guard<std::mutex> lock(mutex);
    if (ok) {
        shared.results = std::move(localResults);
        shared.state = SearchState::Results;
    }
    else {
        shared.error = error;
        shared.state = SearchState::Error;
    }

}

void SoundCloud::startSearch(const std::string &query, Source source) {
    std::lock_guard<std::mutex> lock(mutex);
    if (shared.state == SearchState::Searching) return;
    shared.state = SearchState::Searching;
    shared.error = "";
    if (searchThread.joinable()) searchThread.join();
    searchThread = std::thread(&SoundCloud::searchWorker, this, query, source);
}



bool SoundCloud::runDownload(const RemoteTrack &track, const std::filesystem::path &folder, std::string &error) {
    std::string folderstr = folder.string();
    const char* name = track.source == Source::YouTube ? "%(title)s.%(ext)s" : "%(uploader)s - %(title)s.%(ext)s";
    const char* args[] = {"yt-dlp", "-x", "--audio-format", "mp3","--embed-thumbnail", "--embed-metadata", "--no-playlist", "--no-warnings", "-P", folderstr.c_str(), "-o", name, "--", track.url.c_str(), nullptr};
    SDL_Process* process = SDL_CreateProcess(args, true);
    if (process == nullptr) {
        error = "Could not start yt-dlp";
        return false;
    }
    size_t size;
    int exitcode = -1;
    void* data = SDL_ReadProcess(process, &size, &exitcode);
    if (data != nullptr) {
        SDL_free(data);
    }
    SDL_DestroyProcess(process);
    if (exitcode != 0) error = "Exitcode error";
    return exitcode == 0;
}

void SoundCloud::downloadWorker(RemoteTrack track, std::filesystem::path folder) {
    std::string error;
    bool ok = runDownload(track, folder, error);
    std::lock_guard<std::mutex> lock(mutex);
    for (Download& download : shared.downloads) {
        if (download.track.id == track.id){
            if (ok) download.state = DownloadState::Done;
            else download.state = DownloadState::Failed;
            break;
        }
    }
    if (ok) downloadFinished = true;
}

bool SoundCloud::takeFinishedDownloads() {
    std::lock_guard<std::mutex> lock(mutex);
    bool finished = downloadFinished;
    downloadFinished = false;
    return finished;
}

void SoundCloud::startDownload(const RemoteTrack &track) {
    if (downloadFolder.empty()) return;
    {
        std::lock_guard<std::mutex> lock(mutex);
        bool found = false;
        for (Download& download : shared.downloads) {
            if (download.track.id == track.id) {
                if (download.state != DownloadState::Failed) return;
                download.state = DownloadState::Downloading;
                found = true;
                break;
            }
        }
        if (!found) shared.downloads.push_back(Download{track});
    }
    downloadThreads.emplace_back(&SoundCloud::downloadWorker, this, track, downloadFolder);
}

void SoundCloud::setDownloadFolder(const std::filesystem::path &folder) {
    downloadFolder = folder;
}

SoundCloud::~SoundCloud() {
    if (searchThread.joinable()) searchThread.join();
    for (std::thread& thread : downloadThreads) {
        if (thread.joinable()) thread.join();
    }
}
