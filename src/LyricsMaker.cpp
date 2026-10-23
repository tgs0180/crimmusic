#include "LyricsMaker.hpp"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <exception>
#include <fstream>
#include <sstream>

std::filesystem::path LyricsMaker::workFolder() {
    const char* home = std::getenv("HOME");
    std::filesystem::path folder = std::filesystem::path(home ? home : ".") / ".cache" / "crimmusic" / "make-lyrics";
    std::error_code error;
    std::filesystem::create_directories(folder, error);
    return folder;
}

bool LyricsMaker::available() {
    std::error_code error;
    return std::filesystem::exists("tools/lyrics/align_text.py", error) && std::filesystem::exists("tools/lyrics/run.sh", error);
}

bool LyricsMaker::start(const std::filesystem::path& song, const std::string& lyrics) {
    {
        std::lock_guard<std::mutex> lock(mutex);
        if (shared.state == MakerState::Running) return false;
    }
    if (thread.joinable()) thread.join();

    std::filesystem::path folder = workFolder();
    std::ofstream input(folder / "input.txt");
    if (!input) return false;
    input << lyrics;
    input.close();

    {
        std::lock_guard<std::mutex> lock(mutex);
        shared = MakerResult{};
        shared.state = MakerState::Running;
        shared.song = song;
    }
    thread = std::thread(&LyricsMaker::worker, this, song);
    return true;
}

MakerResult LyricsMaker::snapshot() {
    std::lock_guard<std::mutex> lock(mutex);
    return shared;
}

void LyricsMaker::worker(std::filesystem::path song) {
    std::filesystem::path folder = workFolder();
    std::string songText = song.string();
    std::string inputText = (folder / "input.txt").string();
    std::string lrcText = (folder / "output.lrc").string();
    std::string jsonText = (folder / "result.json").string();
    std::error_code error;
    std::filesystem::remove(folder / "output.lrc", error);
    std::filesystem::remove(folder / "result.json", error);

    const char* argv[] = { "sh", "tools/lyrics/run.sh", "tools/lyrics/align_text.py",
                           songText.c_str(), inputText.c_str(), lrcText.c_str(), jsonText.c_str(), nullptr };
    SDL_Process* process = SDL_CreateProcess(argv, false);
    if (process == nullptr) {
        std::lock_guard<std::mutex> lock(mutex);
        shared.state = MakerState::Failed;
        shared.message = "Could not start the lyrics tool.";
        return;
    }

    int exitCode = -1;
    while (!SDL_WaitProcess(process, false, &exitCode)) {
        bool quitting;
        {
            std::lock_guard<std::mutex> lock(mutex);
            quitting = stopping;
        }
        if (quitting) {
            SDL_KillProcess(process, true);
            SDL_WaitProcess(process, true, &exitCode);
            SDL_DestroyProcess(process);
            return;
        }
        SDL_Delay(100);
    }
    SDL_DestroyProcess(process);

    MakerResult result;
    result.song = song;
    std::ifstream jsonFile(jsonText);
    if (!jsonFile) {
        result.state = MakerState::Failed;
        result.message = "The lyrics tool stopped without a result (exit code " + std::to_string(exitCode) + ").";
    }
    else {
        try {
            nlohmann::json json = nlohmann::json::parse(jsonFile);
            if (json.value("ok", false)) {
                result.state = MakerState::Done;
                result.matches = json.value("matches", false);
                result.words = json.value("words", 0.0f);
                result.coverage = json.value("coverage", 0.0f);
                result.lines = json.value("lines", 0);
                result.weakLines = json.value("weak_lines", 0);
                if (json.contains("dropped") && json["dropped"].is_array()) {
                    for (const auto& line : json["dropped"]) {
                        if (line.is_string()) result.dropped.push_back(line.get<std::string>());
                    }
                }
                std::ifstream lrcFile(lrcText);
                std::stringstream contents;
                contents << lrcFile.rdbuf();
                result.lrc = contents.str();
            }
            else {
                result.state = MakerState::Failed;
                result.message = json.value("message", std::string("Something went wrong."));
            }
        }
        catch (const std::exception&) {
            result.state = MakerState::Failed;
            result.message = "Could not read the result of the lyrics tool.";
        }
    }

    std::lock_guard<std::mutex> lock(mutex);
    shared = std::move(result);
}

LyricsMaker::~LyricsMaker() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    if (thread.joinable()) thread.join();
}
