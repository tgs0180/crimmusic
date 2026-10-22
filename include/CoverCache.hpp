#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <deque>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <thread>

struct SDL_Renderer;
struct SDL_Texture;

class CoverCache {
public:
    static constexpr int coverSize = 160;

    CoverCache();
    ~CoverCache();
    CoverCache(const CoverCache&) = delete;
    CoverCache& operator=(const CoverCache&) = delete;

    void request(const std::filesystem::path& file);
    SDL_Texture* get(const std::filesystem::path& file) const;
    void update(SDL_Renderer* renderer);
    void clear();

private:
    struct Pixels {
        std::filesystem::path file;
        std::vector<unsigned char> rgba;    // empty = this song has no usable cover
    };

    void worker();
    static bool loadCover(const std::filesystem::path& file, std::vector<unsigned char>& out);

    // only touched by the main thread
    std::map<std::filesystem::path, SDL_Texture*> textures;
    std::set<std::filesystem::path> requested;

    // shared with the worker, protected by mutex
    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::filesystem::path> todo;
    std::vector<Pixels> done;
    bool stopping = false;

    std::thread thread;    // declared LAST
};
