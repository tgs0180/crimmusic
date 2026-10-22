#pragma once

struct sd_bus;
class Player;
class Library;

class Mpris {
public:
    Mpris() = default;
    ~Mpris();
    Mpris(const Mpris&) = delete;
    Mpris& operator=(const Mpris&) = delete;

    bool init(Player& player, Library& library);
    void poll();

    Player* player = nullptr;
    Library* library = nullptr;

private:
    sd_bus* bus = nullptr;
};
