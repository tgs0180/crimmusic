#include "IconSet.hpp"
#include <SDL3/SDL.h>
#include <stb_image.h>
#include <iostream>

bool IconSet::load(SDL_Renderer *renderer, const std::filesystem::path &folder) {
    const char* names[] = { "play", "pause", "skip-back", "skip-forward", "repeat", "repeat-1", "repeat-off", "shuffle", "list-video" };
    bool allLoaded = true;
    for (const char* name : names) {
        std::string path = (folder / (std::string(name) + ".png")).string();
        int w;
        int h;
        int channels;
        unsigned char* pixels = stbi_load(path.c_str(), &w, &h, &channels, 4);
        if (pixels == nullptr) {
            std::cerr << "Could not load icon: " << path << "\n";
            allLoaded = false;
            continue;
        }
        SDL_Texture* tex = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
        if (tex == nullptr) {
            stbi_image_free(pixels);
            allLoaded = false;
            continue;
        }
        SDL_UpdateTexture(tex, nullptr, pixels, w*4);
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        textures[name] = tex;
        stbi_image_free(pixels);
    }
    return allLoaded;
}

SDL_Texture* IconSet::get(const std::string &name) const {
    auto it = textures.find(name);
    if (it == textures.end()) return nullptr;
    return it->second;
}

void IconSet::clear() {
    for (auto& entry : textures) {
        if (entry.second != nullptr) SDL_DestroyTexture(entry.second);
    }
    textures.clear();
}
