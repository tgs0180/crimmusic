#pragma once
#include <filesystem>
#include <vector>
#include <cstddef>

class Library {
    public:
        bool scan(const std::filesystem::path& root);
        std::size_t size() const;  
        const std::filesystem::path& song(std::size_t index) const;
        
        std::size_t currentIndex() const;
        const std::filesystem::path& currentSong() const;
        void setCurrent(std::size_t index);
        void next();
        void previous();

    private:
        std::vector<std::filesystem::path> songs;
        std::size_t current = 0;
};