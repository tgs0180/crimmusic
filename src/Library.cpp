#include "Library.hpp"
#include <cstddef>
#include <string>
#include <algorithm>
#include "StringUtils.hpp"
#include <taglib/fileref.h>
#include <taglib/tag.h>

// reads title and artist from the file's tags, with fallbacks if they're missing
static Track readTrack(const std::filesystem::path& file) {
    Track track;
    track.file = file;

    TagLib::FileRef ref(file.c_str());
    if (!ref.isNull() && ref.tag()) {
        track.title = ref.tag()->title().toCString(true);
        track.artist = ref.tag()->artist().toCString(true);
    }
    if (track.title.empty()) track.title = file.stem().string();
    if (track.artist.empty()) track.artist = "Unknown";
    return track;
}

bool Library::scan(const std::filesystem::path& root) {
    std::error_code error;
    if (!std::filesystem::is_directory(root, error)) return false;

    std::vector<Track> waitlist;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(root, std::filesystem::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::string extResult = toLower(extension);
        if (extResult != ".mp3") continue;
        waitlist.push_back(readTrack(entry.path()));
    }
    if (waitlist.empty()) return false;

    // remember which song was current, so a rescan doesn't lose our place
    std::filesystem::path previous;
    if (!songs.empty()) previous = songs[current].file;

    std::sort(waitlist.begin(), waitlist.end(),[](const Track& a, const Track& b) { return a.file < b.file; });
    songs = std::move(waitlist);
    folder = root;

    current = 0;
    for (std::size_t i = 0; i < songs.size(); i++) {
        if (songs[i].file == previous) {
            current = i;
            break;
        }
    }
    // the number of songs may have changed, so the old shuffled order is no longer valid
    if (shuffle) buildOrder();
    return true;
}

bool Library::rescan() {
    return !folder.empty() && scan(folder);
}

std::size_t Library::size() const {
    return songs.size();
}

const Track& Library::song(std::size_t index) const {
    return songs[index];
}

std::size_t Library::currentIndex() const {
    return current;
}

const std::filesystem::path& Library::currentSong() const {
    return songs[current].file;
}

void Library::setCurrent(std::size_t index) {
    current = index;
    // start a fresh shuffled order from the song that was picked
    if (shuffle) buildOrder();
}

bool Library::empty() const {
    return songs.empty();
}

void Library::next() {
    if (songs.empty()) return;
    if (shuffle) {
        orderPosition = (orderPosition + 1) % order.size();
        current = order[orderPosition];
    }
    else current = (current + 1) % songs.size();
}

void Library::previous() {
    if (songs.empty()) return;
    if (shuffle) {
        if (orderPosition == 0) orderPosition = order.size() - 1;
        else orderPosition -= 1;
        current = order[orderPosition];
    }
    else {
        if (current == 0) current = songs.size() - 1;
        else current -= 1;
    }
}

RepeatMode Library::repeatMode() const {
    return repeat;
}

void Library::setRepeatMode(enum RepeatMode mode) {
    repeat = mode;
}

bool Library::atEnd() const {
    if (songs.empty()) return true;
    if (shuffle) return orderPosition == order.size() -1;
    else return current == songs.size() - 1;
}

void Library::buildOrder() {
    order.resize(songs.size());
    std::iota(order.begin(), order.end(), 0);
    std::shuffle(order.begin(), order.end(), rng);
    for (std::size_t i = 0; i < order.size(); i++) {
        if (order[i] == current) {std::swap(order[0], order[i]); break;}
    }
    orderPosition = 0;
}

bool Library::shuffled() const {
    return shuffle;
}

void Library::setShuffle(bool on) {
    shuffle = on;
    if (shuffle) buildOrder();
}