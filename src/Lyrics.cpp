#include "Lyrics.hpp"
#include "StringUtils.hpp"
#include <SDL3/SDL.h>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <exception>
#include <fstream>
#include <sstream>

static std::string trim(const std::string& text) {
    std::size_t start = text.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    std::size_t end = text.find_last_not_of(" \t\r\n");
    return text.substr(start, end - start + 1);
}

static std::string withoutBrackets(const std::string& text) {
    std::string out;
    int depth = 0;
    for (char c : text) {
        if (c == '[') {
            depth++;
            continue;
        }
        if (c == ']' && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0) out += c;
    }
    std::string collapsed;
    for (char c : out) {
        if (c == ' ' && !collapsed.empty() && collapsed.back() == ' ') continue;
        collapsed += c;
    }
    collapsed = trim(collapsed);
    while (!collapsed.empty() && (collapsed.front() == ',' || collapsed.front() == ' ')) collapsed.erase(collapsed.begin());
    return collapsed;
}

static std::string cleanTitle(const std::string& title, const std::string& artist) {
    std::string out;
    int depth = 0;
    for (char c : title) {
        if (c == '(' || c == '[') {
            depth++;
            continue;
        }
        if ((c == ')' || c == ']') && depth > 0) {
            depth--;
            continue;
        }
        if (depth == 0) out += c;
    }

    std::string lower = toLower(out);
    const char* cuts[] = { " prod", " ft.", " feat" };
    for (const char* cut : cuts) {
        std::size_t pos = lower.find(cut);
        if (pos != std::string::npos) {
            out = out.substr(0, pos);
            lower = lower.substr(0, pos);
        }
    }

    std::string prefix = toLower(artist) + " - ";
    if (lower.rfind(prefix, 0) == 0) out = out.substr(prefix.size());

    out = trim(out);
    while (!out.empty() && (out.back() == '-' || out.back() == ' ')) out.pop_back();
    return out;
}

static std::string syncedFrom(const nlohmann::json& entry) {
    if (entry.contains("syncedLyrics") && entry["syncedLyrics"].is_string()) {
        return entry["syncedLyrics"].get<std::string>();
    }
    return "";
}

Lyrics::Lyrics() {
    thread = std::thread(&Lyrics::worker, this);
}

Lyrics::~Lyrics() {
    clear();
}

std::vector<LyricLine> Lyrics::parse(const std::string& lrc) {
    std::vector<LyricLine> lines;
    std::istringstream stream(lrc);
    std::string line;
    while (std::getline(stream, line)) {
        std::vector<float> times;
        std::size_t pos = 0;
        while (pos < line.size() && line[pos] == '[') {
            std::size_t close = line.find(']', pos);
            if (close == std::string::npos) break;
            std::string tag = line.substr(pos + 1, close - pos - 1);
            std::size_t colon = tag.find(':');
            if (colon == std::string::npos) break;
            try {
                int minutes = std::stoi(tag.substr(0, colon));
                float seconds = std::stof(tag.substr(colon + 1));
                times.push_back(minutes * 60.0f + seconds);
            }
            catch (const std::exception&) {
                break;
            }
            pos = close + 1;
        }
        if (times.empty()) continue;
        std::string text = withoutBrackets(trim(line.substr(pos)));
        for (float time : times) {
            lines.push_back(LyricLine{time, text});
        }
    }
    std::stable_sort(lines.begin(), lines.end(), [](const LyricLine& a, const LyricLine& b) { return a.time < b.time; });
    return lines;
}

std::filesystem::path Lyrics::lrcPath(const std::filesystem::path& song) {
    std::filesystem::path lrc = song;
    lrc.replace_extension(".lrc");
    return lrc;
}


const std::vector<LyricLine>& Lyrics::linesFor(const std::filesystem::path& song, const std::string& artist, const std::string& title) {
    static const std::vector<LyricLine> none;

    auto it = cache.find(song);
    if (it != cache.end()) return it->second;
    if (requested.count(song) != 0) return none;

    std::ifstream file(lrcPath(song));
    if (file) {
        std::stringstream contents;
        contents << file.rdbuf();
        store(song, contents.str());
        return cache[song];
    }

    auto retry = retryAt.find(song);
    if (retry != retryAt.end() && std::chrono::steady_clock::now() < retry->second) return none;

    requested.insert(song);
    {
        std::lock_guard<std::mutex> lock(mutex);
        todo.push_front(Request{song, artist, title});
    }
    wake.notify_one();
    return none;
}

void Lyrics::store(const std::filesystem::path& song, const std::string& lrc) {
    cache[song] = parse(lrc);
    if (lrc.find("[source:transcribed]") != std::string::npos) transcribed.insert(song);
    else transcribed.erase(song);
}

bool Lyrics::isTranscribed(const std::filesystem::path& song) const {
    return transcribed.count(song) != 0;
}

void Lyrics::forget(const std::filesystem::path& song) {
    cache.erase(song);
    transcribed.erase(song);
    retryAt.erase(song);
}

void Lyrics::update() {
    std::vector<Result> finished;
    {
        std::lock_guard<std::mutex> lock(mutex);
        finished.swap(done);
    }
    for (Result& result : finished) {
        requested.erase(result.song);
        if (result.outcome == Outcome::Found) {
            store(result.song, result.lrc);
        }
        else if (result.outcome == Outcome::NotFound) {
            cache[result.song] = {};
        }
        else {
            retryAt[result.song] = std::chrono::steady_clock::now() + std::chrono::seconds(30);
        }
    }
}

void Lyrics::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    wake.notify_all();
    if (thread.joinable()) thread.join();
}

void Lyrics::worker() {
    while (true) {
        Request request;
        {
            std::unique_lock<std::mutex> lock(mutex);
            wake.wait(lock, [this] { return stopping || !todo.empty(); });
            if (stopping) return;
            request = todo.front();
            todo.pop_front();
        }

        std::string lrc;
        Outcome outcome;
        if (lyricsToolAvailable()) {
            outcome = runLyricsTool(request, lrc);
        }
        else {
            outcome = fetch(request, lrc);
            std::error_code error;
            if (outcome == Outcome::Found && !std::filesystem::exists(lrcPath(request.song), error)) {
                std::ofstream out(lrcPath(request.song));
                if (out) out << lrc;
            }
        }

        std::lock_guard<std::mutex> lock(mutex);
        done.push_back(Result{request.song, outcome, std::move(lrc)});
    }
}

bool Lyrics::lyricsToolAvailable() {
    std::error_code error;
    return std::filesystem::exists("tools/lyrics/make_lrc.py", error) && std::filesystem::exists("tools/lyrics/run.sh", error);
}

Lyrics::Outcome Lyrics::runLyricsTool(const Request& request, std::string& lrc) {
    std::string song = request.song.string();
    const char* argv[] = { "sh", "tools/lyrics/run.sh", "tools/lyrics/make_lrc.py", song.c_str(),
                           "--artist", request.artist.c_str(), "--title", request.title.c_str(), nullptr };
    SDL_Process* process = SDL_CreateProcess(argv, false);
    if (process == nullptr) return Outcome::Failed;

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
            return Outcome::Failed;
        }
        SDL_Delay(100);
    }
    SDL_DestroyProcess(process);

    if (exitCode == 2) return Outcome::NotFound;
    if (exitCode != 0) return Outcome::Failed;
    std::ifstream file(lrcPath(request.song));
    if (!file) return Outcome::Failed;
    std::stringstream contents;
    contents << file.rdbuf();
    lrc = contents.str();
    return Outcome::Found;
}

int Lyrics::runCurl(const std::vector<std::string>& args, std::string& body) {
    std::vector<const char*> argv;
    argv.push_back("curl");
    argv.push_back("-s");
    argv.push_back("-m");
    argv.push_back("8");
    argv.push_back("-A");
    argv.push_back("crimmusic (https://github.com/tgs0180/crimmusic)");
    argv.push_back("-w");
    argv.push_back("\n%{http_code}");
    argv.push_back("-G");
    for (const std::string& arg : args) argv.push_back(arg.c_str());
    argv.push_back(nullptr);

    SDL_Process* process = SDL_CreateProcess(argv.data(), true);
    if (process == nullptr) return 0;
    size_t size = 0;
    int exitCode = -1;
    std::string output;
    void* data = SDL_ReadProcess(process, &size, &exitCode);
    if (data != nullptr) {
        output.assign(static_cast<char*>(data), size);
        SDL_free(data);
    }
    SDL_DestroyProcess(process);
    if (exitCode != 0) return 0;

    std::size_t newline = output.rfind('\n');
    if (newline == std::string::npos) return 0;
    body = output.substr(0, newline);
    try {
        return std::stoi(output.substr(newline + 1));
    }
    catch (const std::exception&) {
        return 0;
    }
}

Lyrics::Outcome Lyrics::fetch(const Request& request, std::string& lrc) {
    std::string cleaned = cleanTitle(request.title, request.artist);
    std::vector<std::string> titles = { request.title };
    if (!cleaned.empty() && cleaned != request.title) titles.push_back(cleaned);
    bool failed = false;

    for (const std::string& title : titles) {
        {
            std::lock_guard<std::mutex> lock(mutex);
            if (stopping) return Outcome::Failed;
        }
        std::string body;
        int status = runCurl({ "https://lrclib.net/api/get",
                               "--data-urlencode", "artist_name=" + request.artist,
                               "--data-urlencode", "track_name=" + title }, body);
        if (status == 404) continue;
        if (status != 200) {
            failed = true;
            continue;
        }
        try {
            lrc = syncedFrom(nlohmann::json::parse(body));
            if (!lrc.empty()) return Outcome::Found;
        }
        catch (const std::exception&) {
        }
    }

    {
        std::lock_guard<std::mutex> lock(mutex);
        if (stopping) return Outcome::Failed;
    }
    std::string body;
    std::string wantedTitle = cleaned.empty() ? request.title : cleaned;
    int status = runCurl({ "https://lrclib.net/api/search", "--data-urlencode", "q=" + request.artist + " " + wantedTitle }, body);
    if (status != 200) return Outcome::Failed;
    try {
        nlohmann::json results = nlohmann::json::parse(body);
        if (results.is_array()) {
            std::string wantedArtist = toLower(request.artist);
            std::string wantedLower = toLower(wantedTitle);
            for (const nlohmann::json& entry : results) {
                if (!entry.contains("artistName") || !entry["artistName"].is_string()) continue;
                if (!entry.contains("trackName") || !entry["trackName"].is_string()) continue;
                std::string foundArtist = toLower(entry["artistName"].get<std::string>());
                std::string foundTitle = toLower(entry["trackName"].get<std::string>());
                bool sameArtist = foundArtist.find(wantedArtist) != std::string::npos || wantedArtist.find(foundArtist) != std::string::npos;
                bool sameTitle = foundTitle.find(wantedLower) != std::string::npos || wantedLower.find(foundTitle) != std::string::npos;
                if (!sameArtist || !sameTitle) continue;
                lrc = syncedFrom(entry);
                if (!lrc.empty()) return Outcome::Found;
            }
        }
    }
    catch (const std::exception&) {
    }
    return failed ? Outcome::Failed : Outcome::NotFound;
}
