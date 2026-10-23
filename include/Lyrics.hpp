#pragma once
#include <chrono>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <map>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

struct LyricLine {
    float time;
    std::string text;
};

class Lyrics {
    public:
        Lyrics();
        ~Lyrics();
        Lyrics(const Lyrics&) = delete;
        Lyrics& operator=(const Lyrics&) = delete;

        const std::vector<LyricLine>& linesFor(const std::filesystem::path& song, const std::string& artist, const std::string& title);
        bool isTranscribed(const std::filesystem::path& song) const;
        void forget(const std::filesystem::path& song);
        void update();
        void clear();

        static std::vector<LyricLine> parse(const std::string& lrc);

    private:
        struct Request {
            std::filesystem::path song;
            std::string artist;
            std::string title;
        };
        enum class Outcome { Found, NotFound, Failed };
        struct Result {
            std::filesystem::path song;
            Outcome outcome;
            std::string lrc;
        };

        void worker();
        Outcome fetch(const Request& request, std::string& lrc);
        Outcome runLyricsTool(const Request& request, std::string& lrc);
        static bool lyricsToolAvailable();
        static int runCurl(const std::vector<std::string>& args, std::string& body);
        static std::filesystem::path lrcPath(const std::filesystem::path& song);
        void store(const std::filesystem::path& song, const std::string& lrc);

        std::map<std::filesystem::path, std::vector<LyricLine>> cache;
        std::set<std::filesystem::path> transcribed;
        std::set<std::filesystem::path> requested;
        std::map<std::filesystem::path, std::chrono::steady_clock::time_point> retryAt;

        std::mutex mutex;
        std::condition_variable wake;
        std::deque<Request> todo;
        std::vector<Result> done;
        bool stopping = false;

        std::thread thread;
};
