#include "Discord.hpp"
#include "Library.hpp"
#include "Player.hpp"
#include <nlohmann/json.hpp>
#include <SDL3/SDL.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <exception>

static const char* CLIENT_ID = "1563886101620457545";

static std::string youtubeThumbnail(const std::string& url) {
    if (url.find("youtube.com") == std::string::npos) return "";
    std::size_t start = url.find("v=");
    if (start == std::string::npos) return "";
    start += 2;
    std::size_t end = url.find('&', start);
    std::string id = url.substr(start, end == std::string::npos ? std::string::npos : end - start);
    return "https://i.ytimg.com/vi/" + id + "/hqdefault.jpg";
}

static std::string soundcloudThumbnail(std::string url) {
    std::string urlArg = "url=" + url;
    const char* args[] = { "curl", "-s", "-m", "5", "-G", "https://soundcloud.com/oembed",
                           "--data-urlencode", "format=json", "--data-urlencode", urlArg.c_str(), nullptr };
    SDL_Process* process = SDL_CreateProcess(args, true);
    if (process == nullptr) return "";
    size_t size = 0;
    int exitCode = -1;
    void* data = SDL_ReadProcess(process, &size, &exitCode);
    std::string output;
    if (data != nullptr) {
        output.assign(static_cast<char*>(data), size);
        SDL_free(data);
    }
    SDL_DestroyProcess(process);
    if (exitCode != 0) return "";
    try {
        std::string thumbnail = nlohmann::json::parse(output).value("thumbnail_url", "");
        if (thumbnail.find("placeholder") != std::string::npos) return "";
        return thumbnail;
    }
    catch (const std::exception&) {
        return "";
    }
}

bool Discord::connectToDiscord() {
    const char* runtime = std::getenv("XDG_RUNTIME_DIR");
    if (runtime == nullptr) return false;
    for (int i = 0; i < 10; i++) {
        std::string path = std::string(runtime) + "/discord-ipc-" + std::to_string(i);
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0) return false;
        sockaddr_un address{};
        address.sun_family = AF_UNIX;
        std::strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        if (connect(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0) {
            sock = fd;
            nlohmann::json hello = {{"v", 1}, {"client_id", CLIENT_ID}};
            return sendFrame(0, hello.dump());
        }
        close(fd);
    }
    return false;
}

void Discord::disconnect() {
    if (sock >= 0) close(sock);
    sock = -1;
}

bool Discord::sendFrame(std::uint32_t opcode, const std::string& json) {
    std::uint32_t header[2] = { opcode, static_cast<std::uint32_t>(json.size()) };
    std::string frame(reinterpret_cast<const char*>(header), sizeof(header));
    frame += json;
    ssize_t sent = send(sock, frame.data(), frame.size(), MSG_NOSIGNAL);
    if (sent != static_cast<ssize_t>(frame.size())) {
        disconnect();
        return false;
    }
    return true;
}

void Discord::drain() {
    char buffer[4096];
    while (true) {
        ssize_t got = recv(sock, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (got > 0) continue;
        if (got == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) disconnect();
        return;
    }
}

void Discord::sendActivity(Player& player, Library& library, bool playing) {
    nlohmann::json activity = nullptr;
    if (playing) {
        const Track& track = library.song(library.currentIndex());
        long long nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count();
        long long start = nowMs - static_cast<long long>(player.position() * 1000.0f);
        long long end = start + static_cast<long long>(player.length() * 1000.0f);
        std::string title = track.title;
        std::string artist = track.artist;
        while (title.size() < 2) title += ' ';
        while (artist.size() < 2) artist += ' ';
        activity = {
            {"type", 2},
            {"details", title},
            {"state", artist},
            {"timestamps", {{"start", start}, {"end", end}}},
            {"assets", {{"large_image", coverUrl.empty() ? std::string("logo") : coverUrl},
                        {"large_text", artist + " - " + title},
                        {"small_image", "logo"},
                        {"small_text", "crimmusic"}}}
        };
    }
    nlohmann::json message = {
        {"cmd", "SET_ACTIVITY"},
        {"nonce", std::to_string(++nonce)},
        {"args", {{"pid", static_cast<int>(getpid())}, {"activity", activity}}}
    };
    sendFrame(1, message.dump());
}

void Discord::poll(Player& player, Library& library) {
    auto now = std::chrono::steady_clock::now();
    if (sock < 0) {
        if (now < nextConnectTry) return;
        nextConnectTry = now + std::chrono::seconds(15);
        if (!connectToDiscord()) return;
        sentSong.clear();
    }
    drain();
    if (sock < 0 || library.empty()) return;

    const std::filesystem::path& song = library.currentSong();
    if (song != coverSong) {
        coverSong = song;
        const Track& track = library.song(library.currentIndex());
        coverUrl = youtubeThumbnail(track.url);
        if (coverUrl.empty() && track.url.find("soundcloud.com") != std::string::npos) {
            coverLookup = std::async(std::launch::async, soundcloudThumbnail, track.url);
        }
    }
    if (coverLookup.valid() && coverLookup.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
        coverUrl = coverLookup.get();
        coverChanged = true;
    }

    bool playing = player.isPlaying();
    float elapsed = std::chrono::duration<float>(now - sentAt).count();
    float expected = sentPosition + (sentPlaying ? elapsed : 0.0f);
    bool seeked = playing && std::abs(player.position() - expected) > 2.0f;

    if (song == sentSong && playing == sentPlaying && !seeked && !coverChanged) return;
    if (elapsed < 2.0f && !sentSong.empty()) return;

    sendActivity(player, library, playing);
    coverChanged = false;
    sentSong = song;
    sentPlaying = playing;
    sentPosition = player.position();
    sentAt = now;
}

Discord::~Discord() {
    disconnect();
}
