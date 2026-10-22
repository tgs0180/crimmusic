#include "CoverCache.hpp"
#include <SDL3/SDL_render.h>
#include <mutex>
#include <taglib/mpegfile.h>
#include <taglib/id3v2tag.h>
#include <taglib/attachedpictureframe.h>
#include <stb_image.h>
#include <stb_image_resize2.h>
#include <SDL3/SDL.h>


bool CoverCache::loadCover(const std::filesystem::path &file, std::vector<unsigned char> &out) {
    TagLib::MPEG::File mp3(file.c_str());
    auto* id3 = mp3.ID3v2Tag();
    if (id3 == nullptr) return false;
    auto frames = id3->frameListMap()["APIC"];
    if (frames.isEmpty()) return false;
    auto bites = static_cast<TagLib::ID3v2::AttachedPictureFrame*>(frames.front())->picture();
    int width = 0;
    int height = 0;
    int channels = 0;
    unsigned char* pixels = stbi_load_from_memory(
    reinterpret_cast<const unsigned char*>(bites.data()),   
    static_cast<int>(bites.size()),                        
    &width, &height, &channels,                     
    4);         
    if (pixels == nullptr) return false;
    out.resize(coverSize * coverSize * 4);
    stbir_resize_uint8_srgb(pixels, width, height, 0, out.data(), coverSize, coverSize, 0, STBIR_RGBA);
    stbi_image_free(pixels);
    return true;
}

CoverCache::CoverCache() {
    thread = std::thread(&CoverCache::worker, this);
}

void CoverCache::worker() {
    while (true) {
        std::filesystem::path file;
    {
        std::unique_lock<std::mutex> lock(mutex);
        wake.wait(lock, [this] {return stopping || !todo.empty();});
        if (stopping) return;
        file = todo.front();
        todo.pop_front();
    }
        std::vector<unsigned char> rgba;
        bool ok = loadCover(file, rgba);
        if (!ok) rgba.clear();
    {
        std::lock_guard<std::mutex> lock(mutex);
        done.push_back(Pixels{file, std::move(rgba)});
    }
    }
}

void CoverCache::request(const std::filesystem::path &file) {
    if (requested.count(file) != 0) return;
    requested.insert(file);
    {
        std::lock_guard<std::mutex> lock(mutex);
        todo.push_back(file);
    }
    wake.notify_one();
}

void CoverCache::clear() {
    {
        std::lock_guard<std::mutex> lock(mutex);
        stopping = true;
    }
    wake.notify_all();
    if (thread.joinable()) thread.join();
    for (auto& entry : textures) {
        if (entry.second != nullptr) SDL_DestroyTexture(entry.second);
    }
    textures.clear();
}

void CoverCache::update(SDL_Renderer *renderer) {
    std::vector<Pixels> finished;
    {
        std::lock_guard<std::mutex> lock(mutex);
        finished.swap(done);
    }
    for (Pixels& p : finished) {
        if (p.rgba.empty()) {textures[p.file] = nullptr; continue;}
        SDL_Texture* texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, coverSize, coverSize);
        if (texture == nullptr) {textures[p.file] = nullptr; continue;}
        SDL_UpdateTexture(texture, nullptr, p.rgba.data(), coverSize*4);
        textures[p.file] = texture;
    }
}

SDL_Texture* CoverCache::get(const std::filesystem::path &file) const {
    auto it = textures.find(file);
    if (it == textures.end()) return nullptr;
    return it->second;
}

CoverCache::~CoverCache() {
    clear();
}