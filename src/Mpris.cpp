#include "Mpris.hpp"
#include "Player.hpp"
#include "Library.hpp"
#include <systemd/sd-bus.h>
#include <cstring>
#include <iostream>


static int getProperty(sd_bus* bus, const char* path, const char* interface, const char* property,
                       sd_bus_message* reply, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);

    if (std::strcmp(property, "PlaybackStatus") == 0) {
        return sd_bus_message_append(reply, "s", mpris->player->isPlaying() ? "Playing" : "Paused");
    }
    if (std::strcmp(property, "Identity") == 0) {
        return sd_bus_message_append(reply, "s", "crimmusic");
    }
    if (std::strcmp(property, "CanQuit") == 0 || std::strcmp(property, "CanRaise") == 0) {
        return sd_bus_message_append(reply, "b", 0);
    }
    return sd_bus_message_append(reply, "b", 1);
}

static int onPlayPause(sd_bus_message* m, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);
    mpris->player->togglePause();
    return sd_bus_reply_method_return(m, "");
}

static int onPlay(sd_bus_message* m, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);
    if (!mpris->player->isPlaying()) mpris->player->togglePause();
    return sd_bus_reply_method_return(m, "");
}

static int onPause(sd_bus_message* m, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);
    if (mpris->player->isPlaying()) mpris->player->togglePause();
    return sd_bus_reply_method_return(m, "");
}

static int onNext(sd_bus_message* m, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);
    if (!mpris->library->empty()) {mpris->library->next(); mpris->player->play(mpris->library->currentSong());}
    return sd_bus_reply_method_return(m, "");
}

static int onPrevious(sd_bus_message* m, void* userdata, sd_bus_error* error)
{
    Mpris* mpris = static_cast<Mpris*>(userdata);
    if (!mpris->library->empty()) {mpris->library->previous(); mpris->player->play(mpris->library->currentSong());}
    return sd_bus_reply_method_return(m, "");
}


static const sd_bus_vtable rootVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_PROPERTY("Identity", "s", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanQuit",  "b", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanRaise", "b", getProperty, 0, 0),
    SD_BUS_VTABLE_END
};

static const sd_bus_vtable playerVtable[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("PlayPause", "", "", onPlayPause, 0),
    SD_BUS_METHOD("Play",      "", "", onPlay,      0),
    SD_BUS_METHOD("Pause",     "", "", onPause,     0),
    SD_BUS_METHOD("Next",      "", "", onNext,      0),
    SD_BUS_METHOD("Previous",  "", "", onPrevious,  0),
    SD_BUS_PROPERTY("PlaybackStatus", "s", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanControl",     "b", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanPlay",        "b", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanPause",       "b", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanGoNext",      "b", getProperty, 0, 0),
    SD_BUS_PROPERTY("CanGoPrevious",  "b", getProperty, 0, 0),
    SD_BUS_VTABLE_END
};

bool Mpris::init(Player &player, Library &library) {
    this->player = &player;
    this->library = &library;

    if (sd_bus_open_user(&bus) < 0) {bus = nullptr; return false;}
    const char* path = "/org/mpris/MediaPlayer2";
    int a = sd_bus_add_object_vtable(bus, nullptr, path, "org.mpris.MediaPlayer2", rootVtable, this);
    int b = sd_bus_add_object_vtable(bus, nullptr, path, "org.mpris.MediaPlayer2.Player", playerVtable, this);
    if (a < 0 || b < 0) return false;
    int c = sd_bus_request_name(bus, "org.mpris.MediaPlayer2.crimmusic", 0);
    if (c < 0) return false;
    return true;
}

void Mpris::poll() {
    if (bus == nullptr) return;
    while (sd_bus_process(bus, nullptr) > 0) {};
}

Mpris::~Mpris() {
    if (bus != nullptr) sd_bus_unref(bus);
}