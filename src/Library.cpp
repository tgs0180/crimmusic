#include "Library.hpp"
#include <cstddef>
#include <string>
#include <algorithm>
#include <cctype>

static std::string toLower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c);});
    return s;
}

bool Library::scan(const std::filesystem::path& root) {
    songs.clear();
    current = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::string extResult = toLower(extension);
        if (extResult != ".mp3") continue;
        songs.push_back(entry.path());
        
    }

    std::sort(songs.begin(), songs.end());
    return !songs.empty();
}

std::size_t Library::size() const {
    return songs.size();
}

const std::filesystem::path& Library::song(std::size_t index) const {
    return songs[index];
}

std::size_t Library::currentIndex() const {
    return current;
}

const std::filesystem::path& Library::currentSong() const {
    return songs[current];
}

void Library::setCurrent(std::size_t index) {
    current = index;
}

void Library::next() {
    current = (current + 1) % songs.size();
}

void Library::previous() {
    if (current == 0) current = songs.size() - 1;
    else current -= 1;
}