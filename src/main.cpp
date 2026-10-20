#include <cstddef>
#include <exception>
#include <iostream>
#include <filesystem>
#include <vector>
#include <string>
#include <algorithm>
#include <cctype>
#include "Player.hpp"
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"


namespace fs = std::filesystem;


std::string toLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {return std::tolower(c);});
    return s;
}

std::vector<fs::path> findMp3s(const fs::path& root)
{
    std::vector<fs::path> songs;

    for (const auto& entry : fs::recursive_directory_iterator(root, fs::directory_options::skip_permission_denied)) {
        if (!entry.is_regular_file()) continue;
        std::string extension = entry.path().extension().string();
        std::string extResult = toLower(extension);
        if (extResult != ".mp3") continue;
        songs.push_back(entry.path());
        
    }

    std::sort(songs.begin(), songs.end());
    return songs;
}

bool playSong(Player& player, const std::vector<fs::path>& songs, std::size_t index)
{
    if (!player.play(songs[index])) return false;
    std::cout << "Playing: " << songs[index].string() << "\n";
    return true;
}

void playNext(Player& player, const std::vector<fs::path>& songs, std::size_t& current)
{
    current = (current + 1) % songs.size();
    playSong(player, songs, current);
}

void playPrevious(Player& player, const std::vector<fs::path>& songs, std::size_t& current)
{
    if (current == 0) current = songs.size() - 1;
    else current -= 1;
    playSong(player, songs, current);
}

int main(int argc, char* argv[]){
    if (argc < 2) {
        std::cerr << "Usage: crimmusic <music folder>"<< "\n";
        return 1;
    }

    fs::path root = argv[1];

    if (!fs::exists(root) || !fs::is_directory(root)) {
        std::cerr << "Not a folder: " << root.string() << "\n";
        return 1;

    }
    std::vector<fs::path> songs = findMp3s(root);
    std::cout << "Found " << songs.size() << " songs" << "\n";
    if (songs.empty()) {
        std::cerr << "ERROR: No songs found" << "\n";
        return 1;
    }
    Player player;
    if (!player.init()) return 1;
    
    if (!playSong(player, songs, 0)) return 1;
    std::size_t current = 0;

    // --- setup ---
    if (!SDL_Init(SDL_INIT_VIDEO)) {           // SDL3: returns true on success (SDL2 was the opposite!)
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_Window* window = SDL_CreateWindow("crimmusic", 900, 600, SDL_WINDOW_RESIZABLE);
    SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
    if (!window || !renderer) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_SetRenderVSync(renderer, 1);           // wait for the screen refresh, otherwise it runs at 100% CPU

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);

    // --- frame loop ---
    bool running = true;
    while (running) {
        // 1. events
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) running = false;
        }

        // 2. start a new ImGui frame
        ImGui_ImplSDLRenderer3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // ===== YOUR UI GOES HERE =====
        if (player.isFinished()) playNext(player, songs, current);

        ImGui::Begin("crimmusic");
        ImGui::Text("Now playing: %s", songs[current].filename().string().c_str());
        if (ImGui::Button("Previous")) {
            playPrevious(player, songs, current);
        }
        ImGui::SameLine();
        if (ImGui::Button("Pause")) {
            player.togglePause();
        }
        ImGui::SameLine();
        if (ImGui::Button("Next")) {
            playNext(player, songs, current);
        }
        for (std::size_t i = 0; i < songs.size(); i++) {
            if (ImGui::Selectable(songs[i].filename().string().c_str(), i == current)) {
                if (playSong(player, songs, i)) current = i;
            }
        }
        ImGui::End();

        // 3. draw
        ImGui::Render();
        SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
        SDL_RenderClear(renderer);
        ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
        SDL_RenderPresent(renderer);
    }

    // --- cleanup, in reverse order of setup ---
    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}

