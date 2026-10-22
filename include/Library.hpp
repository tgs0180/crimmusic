#pragma once
#include <filesystem>
#include <vector>
#include <cstddef>
#include "Track.hpp"

class Library {
    public:
        bool scan(const std::filesystem::path& root);
        bool rescan();
        std::size_t size() const;  
        const Track& song(std::size_t index) const;
        
        std::size_t currentIndex() const;
        const std::filesystem::path& currentSong() const;
        void setCurrent(std::size_t index);
        void next();
        void previous();
        bool empty() const;

    private:
        std::vector<Track> songs;
        std::size_t current = 0;
        std::filesystem::path folder;
};