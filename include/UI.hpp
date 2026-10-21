#pragma once
class Player;
class Library;
struct SDL_Window;
struct SDL_Renderer;

class UI {
    public:
        UI() =default;
        UI(const UI&) = delete;
        UI& operator=(const UI&) = delete;
        ~UI();

        bool init();
        bool processEvents();
        void draw(Player& player, Library& library);

        
    private:
        SDL_Window* window = nullptr;
        SDL_Renderer* renderer = nullptr;
        bool sdlReady = false;
        bool imguiReady = false;
};