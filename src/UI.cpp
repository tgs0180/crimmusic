#include "UI.hpp"
#include "Player.hpp"
#include "Library.hpp"
#include <SDL3/SDL.h>
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <iostream>
#include <cstddef>



bool UI::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {           // SDL3: returns true on success (SDL2 was the opposite!)
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return false;
    }
    sdlReady = true;
    window = SDL_CreateWindow("crimmusic", 900, 600, SDL_WINDOW_RESIZABLE);
    renderer = SDL_CreateRenderer(window, NULL);
    if (!window || !renderer) {
        std::cerr << "Failed to create window: " << SDL_GetError() << "\n";
        return false;
    }
    SDL_SetRenderVSync(renderer, 1);           // wait for the screen refresh, otherwise it runs at 100% CPU

    ImGui::CreateContext();
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    imguiReady = true;
    return true;
}

bool UI::processEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        ImGui_ImplSDL3_ProcessEvent(&event);
        if (event.type == SDL_EVENT_QUIT) return false;
    }
    return true;
}

void UI::draw(Player &player, Library &library) {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("crimmusic", nullptr, flags);
    
    float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() * 2;

    ImGui::BeginChild("songlist", ImVec2(0, -bottomBarHeight), ImGuiChildFlags_Borders);
        for (std::size_t i = 0; i < library.size(); i++) {
            if (ImGui::Selectable((std::to_string(i + 1) + ". " + library.song(i).filename().string()).c_str(), i == library.currentIndex())) {
                library.setCurrent(i);
                player.play(library.currentSong());
            }
        }
    ImGui::EndChild();
    ImGui::Text("Now playing: %s", library.currentSong().filename().string().c_str());
    if (ImGui::Button("Previous")) {
        library.previous();
        player.play(library.currentSong());
    }
    ImGui::SameLine();
    if (player.isPlaying()) {
        if (ImGui::Button("Pause")) {
            player.togglePause();
        }
    }
    else {
        if (ImGui::Button("Resume")) {
            player.togglePause();
        }
    }
    
    
    ImGui::SameLine();
    if (ImGui::Button("Next")) {
        library.next();
        player.play(library.currentSong());
    }
    
    ImGui::End();

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 20, 20, 20, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}

UI::~UI() {
    if (imguiReady) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (sdlReady) SDL_Quit();
}