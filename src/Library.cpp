#include "Library.hpp"
#include <cstddef>
#include <string>
#include <algorithm>
#include "StringUtils.hpp"

bool Library::scan(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return false;

    std::vector<std::filesystem::path> waitlist;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::string extResult = toLower(extension);
        if (extResult != ".mp3") continue;
        waitlist.push_back(entry.path());
    }
    if (waitlist.empty()) return false;
    songs = std::move(waitlist);
    current = 0;

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

bool Library::empty() const {
    return songs.empty();
}

void Library::next() {
    if (songs.empty()) return;
    current = (current + 1) % songs.size();
}

void Library::previous() {
    if (songs.empty()) return;
    if (current == 0) current = songs.size() - 1;
    else current -= 1;
}