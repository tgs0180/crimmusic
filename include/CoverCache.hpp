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
    explicit CoverCache(int size);
    int size() const;
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
        std::vector<unsigned char> rgba;
    };

    void worker();
    static bool loadCover(const std::filesystem::path& file, int size, std::vector<unsigned char>& out);

    const int coverSize;

    std::map<std::filesystem::path, SDL_Texture*> textures;
    std::set<std::filesystem::path> requested;

    std::mutex mutex;
    std::condition_variable wake;
    std::deque<std::filesystem::path> todo;
    std::vector<Pixels> done;
    bool stopping = false;

    std::thread thread;
};
