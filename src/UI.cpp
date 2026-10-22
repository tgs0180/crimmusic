#include "UI.hpp"
#include "Player.hpp"
#include "Library.hpp"
#include <SDL3/SDL.h>
#include "Soundcloud.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL_dialog.h>
#include <iostream>
#include <cstddef>
#include "StringUtils.hpp"
#include <string>

static void splitTime(float seconds, int& minutes, int& secs) {
    int total = static_cast<int>(seconds);
    minutes = total / 60;
    secs = total % 60;
}

void UI::onFolderChosen(void *userdata, const char *const *filelist, int filter){
    if (filelist == nullptr || filelist[0] == nullptr) return;
    UI* ui = static_cast<UI*>(userdata);
    std::lock_guard<std::mutex> lock(ui->folderMutex);
    ui->chosenFolder = filelist[0];
}

bool UI::takeChosenFolder(std::filesystem::path &out) {
    std::lock_guard<std::mutex> lock(folderMutex);   
    if (chosenFolder.empty()) return false;
    out = chosenFolder;
    chosenFolder = "";
    return true;
}

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
    auto fontAtlas = ImGui::GetIO().Fonts;
    ImFontConfig config;
    config.MergeMode = true;
    fontAtlas->AddFontFromFileTTF("/usr/share/fonts/noto/NotoSans-Regular.ttf", 18.0f);
    fontAtlas->AddFontFromFileTTF("/usr/share/fonts/noto-cjk/NotoSansCJK-Regular.ttc", 18.0f, & config);
    fontAtlas->AddFontFromFileTTF("/usr/share/fonts/noto/NotoSansMath-Regular.ttf", 18.0f, &config);
    
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
    if (quit) {
        return false;
    }
    return true;
}
// decides what gets drawn.
void UI::draw(Player &player, Library &library, SoundCloud &soundcloud) {
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
    ImGui::Begin("crimmusic", nullptr, flags);

    if (ImGui::Button("≡##menu")) {
        ImGui::OpenPopup("Menu");
    }
    if (ImGui::BeginPopup("Menu")){
        if (ImGui::MenuItem("Choose folder")) {
            SDL_ShowOpenFolderDialog(onFolderChosen, this, window, nullptr, false);
        }
        ImGui::MenuItem("Settings");
        if (ImGui::MenuItem("Search 3rd party")) {
            showSoundCloud = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Quit")) quit = true;
        
        
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemAllowOverlap();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputTextWithHint("##search", "Search...", searchText, sizeof(searchText));
    std::string query = toLower(searchText);
    float bottomBarHeight = ImGui::GetFrameHeightWithSpacing() * 2;

    bool isEmpty = query.empty();
    if (!isEmpty) {
        float xpos = ImGui::GetItemRectMax().x - 20;
        ImGui::SameLine();
        ImGui::SetCursorPosX(xpos);
        if (ImGui::SmallButton("x##clearsearch")) {
            searchText[0] = '\0';
        }
    }

    ImGui::BeginChild("songlist", ImVec2(0, -bottomBarHeight), ImGuiChildFlags_Borders);
        for (std::size_t i = 0; i < library.size(); i++) {
            std::string songname = library.song(i).filename().string();
            songname = toLower(songname);
            if (!isEmpty && songname.find(query) == std::string::npos) continue;
            if (ImGui::Selectable((std::to_string(i + 1) + ". " + library.song(i).filename().string()).c_str(), i == library.currentIndex())) {
                library.setCurrent(i);
                player.play(library.currentSong());
            }
        }
    ImGui::EndChild();
    if (library.empty()) {
        ImGui::Text("No folder chosen, use the menu to choose one");
    }
    else {
        ImGui::Text("Now playing: %s", library.currentSong().filename().string().c_str());
    }
    if (ImGui::Button("Previous") && !library.empty()) {
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
    if (ImGui::Button("Next") && !library.empty()) {
        library.next();
        player.play(library.currentSong());
    }

    ImGui::SameLine();
    float rightSide = ImGui::CalcTextSize("00:00 / 00:00").x + 150 + ImGui::GetStyle().ItemSpacing.x * 2;
    ImGui::SetNextItemWidth(-rightSide);
    float pos = player.position();
    if (ImGui::SliderFloat("##position", &pos, 0.0f, player.length(), "")) {
        player.seek(pos);
    }
    ImGui::SameLine();
    int posMin, posSec, lenMin, lenSec;
    splitTime(pos, posMin, posSec);
    splitTime(player.length(), lenMin, lenSec);
    ImGui::Text("%d:%02d / %d:%02d", posMin, posSec, lenMin, lenSec);

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    float vol = player.volume() * 100;
    if (ImGui::SliderFloat("##volume", &vol, 0.0f, 100.0f, "%.0f%%")) {
        player.setVolume(vol/100.0f);
    }
    
    ImGui::End();

    if (showSoundCloud) {
        ImGui::Begin("SoundCloud", &showSoundCloud);
        SearchSnapshot snap = soundcloud.snapshot();
        bool busy = (snap.state == SearchState::Searching);
        ImGui::BeginDisabled(busy);
        bool enter = ImGui::InputTextWithHint("##scquery", "Search soundcloud...", soundCloudQuery, sizeof(soundCloudQuery), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        bool clicked = ImGui::Button("Search");
        ImGui::EndDisabled();
        if (enter || clicked) {
            if (soundCloudQuery[0] != '\0') {
                soundcloud.startSearch(soundCloudQuery);
            }
        }
        if (snap.state == SearchState::Searching) ImGui::Text("Searching...");
        else if (snap.state == SearchState::Error) ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", snap.error.c_str());
        else if (snap.state == SearchState::Results && snap.results.empty()) ImGui::Text("No results");
        else if (snap.state == SearchState::Results) {
            if (ImGui::BeginTable("scresults", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Title");
                ImGui::TableSetupColumn("Uploader");
                ImGui::TableSetupColumn("Duration");
                ImGui::TableHeadersRow();
                for (const RemoteTrack& track : snap.results) {
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(track.title.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(track.uploader.c_str());
                    ImGui::TableNextColumn();
                    int minutes, seconds;
                    splitTime(track.duration, minutes, seconds);
                    ImGui::Text("%d:%02d", minutes, seconds);
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

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