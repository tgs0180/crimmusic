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

    // remember which song was current, so a rescan doesn't lose our place
    std::filesystem::path previous;
    if (!songs.empty()) previous = songs[current];

    std::sort(waitlist.begin(), waitlist.end());
    songs = std::move(waitlist);
    folder = root;

    current = 0;
    for (std::size_t i = 0; i < songs.size(); i++) {
        if (songs[i] == previous) {
            current = i;
            break;
        }
    }
    return true;
}

bool Library::rescan() {
    return !folder.empty() && scan(folder);
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