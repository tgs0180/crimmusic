#pragma once
#include <string>
#include <map>
#include <filesystem>

struct SDL_Renderer;
struct SDL_Texture;

class IconSet{
    public:
        IconSet() = default;
        IconSet(const IconSet&) = delete;
        IconSet& operator=(const IconSet&) = delete;

        bool load(SDL_Renderer* renderer, const std::filesystem::path& folder);
        SDL_Texture* get(const std::string& name) const;
        void clear();

    private:
        std::map<std::string, SDL_Texture*> textures;
};
