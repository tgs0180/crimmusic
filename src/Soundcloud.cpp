#include "Soundcloud.hpp"
#include <SDL3/SDL.h>
#include <SDL3/SDL_process.h>
#include <cstddef>
#include <exception>
#include <mutex>
#include <nlohmann/json.hpp>
#include <sstream>


bool SoundCloud::runSearch(const std::string& query, std::vector<RemoteTrack>& out, std::string& error) {
    std::string searchArg = "scsearch5:" + query;
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
            track.duration = json.value("duration", 0.0f);
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

void SoundCloud::searchWorker(std::string query) {
    std::vector<RemoteTrack> localResults;
    std::string error;
    bool ok = runSearch(query, localResults, error);
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

void SoundCloud::startSearch(const std::string &query) {
    std::lock_guard<std::mutex> lock(mutex);
    if (shared.state == SearchState::Searching) return;
    shared.state = SearchState::Searching;
    shared.error = "";
    if (searchThread.joinable()) searchThread.join();
    searchThread = std::thread(&SoundCloud::searchWorker, this, query);
}

SoundCloud::~SoundCloud() {
    if (searchThread.joinable()) searchThread.join();
}
