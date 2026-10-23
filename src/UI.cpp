#include "UI.hpp"
#include "Player.hpp"
#include "Library.hpp"
#include <SDL3/SDL.h>
#include "Soundcloud.hpp"
#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_sdlrenderer3.h"
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_render.h>
#include <cstdint>
#include <iostream>
#include <cstddef>
#include "StringUtils.hpp"
#include <string>

static void splitTime(float seconds, int& minutes, int& secs) {
    int total = static_cast<int>(seconds);
    minutes = total / 60;
    secs = total % 60;
}

static float ButtonWidth(const char* label) {
    return ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2;
}

static float iconButtonWidth() {
    return ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.x * 2;
}

static std::string fitText(const std::string& text, float maximumWidth){
    if (ImGui::CalcTextSize(text.c_str()).x <= maximumWidth) return text;
    std::string looptext = text;
    for (char& c : looptext) {
        looptext.pop_back();
        while (!looptext.empty() && (static_cast<unsigned char>(looptext.back()) & 0xC0) == 0x80) looptext.pop_back();
        if (ImGui::CalcTextSize((looptext + "...").c_str()).x <= maximumWidth || looptext.empty()) return looptext + "...";
    }
    return looptext + "...";
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

// turns a hex colour like 0xe01040 into the 0.0-1.0 floats ImGui uses
static ImVec4 hexColor(unsigned int rgb, float alpha = 1.0f) {
    float r = ((rgb >> 16) & 0xFF) / 255.0f;
    float g = ((rgb >> 8) & 0xFF) / 255.0f;
    float b = (rgb & 0xFF) / 255.0f;
    return ImVec4(r, g, b, alpha);
}

// crimson theme, same palette as the waybar config
namespace palette {
    constexpr unsigned int base    = 0x0d0809;
    constexpr unsigned int surface = 0x1b1114;
    constexpr unsigned int overlay = 0x2a191d;
    constexpr unsigned int muted   = 0x75595f;
    constexpr unsigned int subtle  = 0xa8898f;
    constexpr unsigned int text    = 0xefe1e3;
    constexpr unsigned int crimson = 0xe01040;
    constexpr unsigned int bright  = 0xfc0556;
    constexpr unsigned int blood   = 0x8c0a26;
}

static void applyTheme() {
    using namespace palette;
    ImGuiStyle& style = ImGui::GetStyle();

    style.Colors[ImGuiCol_WindowBg]             = hexColor(base);
    style.Colors[ImGuiCol_ChildBg]              = hexColor(base);
    style.Colors[ImGuiCol_PopupBg]              = hexColor(surface);
    style.Colors[ImGuiCol_Border]               = hexColor(blood, 0.8f);

    style.Colors[ImGuiCol_Text]                 = hexColor(text);
    style.Colors[ImGuiCol_TextDisabled]         = hexColor(subtle);
    style.Colors[ImGuiCol_TextSelectedBg]       = hexColor(crimson, 0.35f);

    style.Colors[ImGuiCol_FrameBg]              = hexColor(overlay);
    style.Colors[ImGuiCol_FrameBgHovered]       = hexColor(crimson, 0.25f);
    style.Colors[ImGuiCol_FrameBgActive]        = hexColor(crimson, 0.4f);

    style.Colors[ImGuiCol_SliderGrab]           = hexColor(crimson);
    style.Colors[ImGuiCol_SliderGrabActive]     = hexColor(bright);
    style.Colors[ImGuiCol_CheckMark]            = hexColor(crimson);

    style.Colors[ImGuiCol_Button]               = hexColor(surface);
    style.Colors[ImGuiCol_ButtonHovered]        = hexColor(crimson, 0.25f);
    style.Colors[ImGuiCol_ButtonActive]         = hexColor(crimson);

    style.Colors[ImGuiCol_Header]               = hexColor(crimson, 0.4f);
    style.Colors[ImGuiCol_HeaderHovered]        = hexColor(crimson, 0.25f);
    style.Colors[ImGuiCol_HeaderActive]         = hexColor(crimson, 0.6f);

    style.Colors[ImGuiCol_TitleBg]              = hexColor(surface);
    style.Colors[ImGuiCol_TitleBgActive]        = hexColor(blood);
    style.Colors[ImGuiCol_TitleBgCollapsed]     = hexColor(surface);

    style.Colors[ImGuiCol_ScrollbarBg]          = hexColor(base);
    style.Colors[ImGuiCol_ScrollbarGrab]        = hexColor(overlay);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = hexColor(muted);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = hexColor(crimson);

    style.Colors[ImGuiCol_TableHeaderBg]        = hexColor(overlay);
    style.Colors[ImGuiCol_TableBorderStrong]    = hexColor(blood, 0.8f);
    style.Colors[ImGuiCol_TableBorderLight]     = hexColor(overlay);
    style.Colors[ImGuiCol_TableRowBg]           = hexColor(base, 0.0f);
    style.Colors[ImGuiCol_TableRowBgAlt]        = hexColor(surface, 0.5f);

    style.Colors[ImGuiCol_Separator]            = hexColor(blood);
    style.Colors[ImGuiCol_SeparatorHovered]     = hexColor(crimson, 0.6f);
    style.Colors[ImGuiCol_SeparatorActive]      = hexColor(crimson);

    style.Colors[ImGuiCol_ResizeGrip]           = hexColor(crimson, 0.2f);
    style.Colors[ImGuiCol_ResizeGripHovered]    = hexColor(crimson, 0.6f);
    style.Colors[ImGuiCol_ResizeGripActive]     = hexColor(bright);

    style.Colors[ImGuiCol_NavCursor]            = hexColor(crimson);

    // rounded corners, like the waybar
    style.FrameRounding  = 6.0f;
    style.GrabRounding   = 6.0f;
    style.ChildRounding  = 6.0f;
    style.PopupRounding  = 8.0f;
    style.WindowRounding = 8.0f;
}

bool UI::iconButton(const char *iconName, bool active) {
    SDL_Texture* tex = icons.get(iconName);
    if (tex == nullptr) return ImGui::Button(iconName);
    float size = ImGui::GetFontSize();
    // white icons get multiplied by the tint: white stays white, crimson makes them crimson
    ImVec4 tint = active ? hexColor(palette::crimson) : ImVec4(1, 1, 1, 1);
    return ImGui::ImageButton(iconName, (ImTextureID)(intptr_t)tex, ImVec2(size, size), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint);
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
    icons.load(renderer, "assets/icons");
    ImGui::CreateContext();
    applyTheme();
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
// ! decides what gets drawn.
void UI::draw(Player &player, Library &library, SoundCloud &soundcloud) {
    covers.update(renderer);
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings / ImGuiWindowFlags_NoBringToFrontOnFocus;
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
        float tile = CoverCache::coverSize;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float available = ImGui::GetContentRegionAvail().x;
        int columns = std::max(1, static_cast<int>((available + spacing) / (tile + spacing)));
        int shown{0};
        for (std::size_t i = 0; i < library.size(); i++) {
            const Track& track = library.song(i);
            std::string label = track.title + " - " + track.artist;
            if (!isEmpty && toLower(label).find(query) == std::string::npos) continue;
            if (shown % columns != 0) ImGui::SameLine();
            shown++;
            ImGui::PushID(static_cast<int>(i));
            ImGui::BeginGroup();
            if (ImGui::IsRectVisible(ImVec2(tile, tile))) covers.request(track.file);
            SDL_Texture* tex = covers.get(track.file);
            bool clicked;
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0,0));
            if (tex != nullptr) clicked = ImGui::ImageButton("##cover", (ImTextureID)(intptr_t)tex, ImVec2(tile, tile));
            else clicked = ImGui::Button("##cover", ImVec2(tile, tile));
            ImGui::PopStyleVar();
            if (i == library.currentIndex()) {
                ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), ImGui::GetColorU32(hexColor(palette::crimson)), 6.0f, 0, 3.0f);
            }
            ImGui::TextUnformatted(fitText(track.title, tile).c_str());
            ImGui::TextDisabled("%s", fitText(track.artist, tile).c_str());
            ImGui::EndGroup();
            ImGui::PopID();
            if (clicked) {
                library.setCurrent(i);
                player.play(library.currentSong());
            }
            
        }
    ImGui::EndChild();
    if (library.empty()) {
        ImGui::Text("No folder chosen, use the menu to choose one");
    }
    else {
        const Track& playing = library.song(library.currentIndex());
        ImGui::Text("Now playing: %s - %s", playing.title.c_str(), playing.artist.c_str());
        if (ImGui::IsItemHovered()) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        if (ImGui::IsItemClicked()) {NowPlayingOpen = true; JustOpened = true;;}
    }
    if (iconButton("skip-back") && !library.empty()) {
        library.previous();
        player.play(library.currentSong());
    }

    ImGui::SameLine();
    if (iconButton(player.isPlaying() ? "pause" : "play")) {
        player.togglePause();
    }

    ImGui::SameLine();
    if (iconButton("skip-forward") && !library.empty()) {
        library.next();
        player.play(library.currentSong());
    }

    ImGui::SameLine();
    float rightSide = ImGui::CalcTextSize("00:00 / 00:00").x + 150 + ImGui::GetStyle().ItemSpacing.x * 5 + iconButtonWidth()*3;
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
    
    RepeatMode repeatmode = library.repeatMode();
    const char* repeatstr;
    if (repeatmode == RepeatMode::Off) repeatstr = "repeat-off";
    else if (repeatmode == RepeatMode::All) repeatstr = "repeat";
    else if (repeatmode == RepeatMode::One) repeatstr = "repeat-1";

    ImGui::SameLine();
    if (iconButton(repeatstr, repeatmode != RepeatMode::Off)) {
        if (library.repeatMode() == RepeatMode::All) library.setRepeatMode(RepeatMode::Off);
        else if (library.repeatMode() == RepeatMode::Off) library.setRepeatMode(RepeatMode::One);
        else if (library.repeatMode() == RepeatMode::One) library.setRepeatMode(RepeatMode::All);
    }

    ImGui::SameLine();
    if (iconButton("shuffle", library.shuffled())) {
        library.setShuffle(!library.shuffled());
    }

    ImGui::SameLine();
    if (iconButton("list-video")) {
        ImGui::OpenPopup("queue");
    }
    ImGui::SetItemTooltip("Queue");
    ImGui::SetNextWindowPos(ImGui::GetItemRectMin(), ImGuiCond_Appearing, ImVec2(0.5f, 1.0f));
    if (ImGui::BeginPopup("queue")) {
        ImGui::TextDisabled("Up next");
        ImGui::Separator();
        ImGui::BeginChild("Queuelist", ImVec2(500, 400));
        std::vector<std::size_t> next = library.upcoming(library.size());
        if (next.empty()) ImGui::Text("It's quiet in here...");
        else {
            float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2;
            float coverSize = 32.0f;
            // the clipper only hands out the rows that are actually visible in the scroll area
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(next.size()));
            while (clipper.Step()) {
            for (int k = clipper.DisplayStart; k < clipper.DisplayEnd; k++) {
                const Track& t = library.song(next[static_cast<std::size_t>(k)]);
                ImGui::PushID(k);
                ImVec2 start = ImGui::GetCursorPos();
                bool clicked = ImGui::Selectable("##row", false, 0, ImVec2(0, rowHeight));
                ImGui::SetCursorPos(start);
                covers.request(t.file);
                SDL_Texture* tex = covers.get(t.file);
                if (tex) {
                    ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(coverSize, coverSize));
                }
                else {
                    ImGui::Dummy(ImVec2(coverSize, coverSize));
                }
                ImGui::SameLine();
                ImGui::BeginGroup();
                auto textwidth = ImGui::GetContentRegionAvail().x;
                ImGui::TextUnformatted(fitText(t.title, textwidth).c_str());
                ImGui::TextDisabled("%s", fitText(t.artist, textwidth).c_str());
                ImGui::EndGroup();
                if (clicked) {
                    int x = k + 1;
                    while(x != 0) {
                        library.next();
                        x -= 1;
                    }
                    player.play(library.currentSong());
                }
                ImGui::PopID();
            }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(150);
    float vol = player.volume() * 100;
    if (ImGui::SliderFloat("##volume", &vol, 0.0f, 100.0f, "%.0f%%")) {
        player.setVolume(vol/100.0f);
    }
    
    ImGui::End();

    if (NowPlayingOpen) {
        if (JustOpened) {
            ImGui::SetNextWindowFocus();
            JustOpened = false;
        }
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;

        ImGui::Begin("nowplaying", nullptr, flags);

        if (ImGui::Button("X")) NowPlayingOpen = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) NowPlayingOpen = false;

        ImGui::End();
    }

    if (showSoundCloud) {
        ImGui::SetNextWindowSize(ImVec2(700, 400), ImGuiCond_Appearing);
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
            if (ImGui::BeginTable("scresults", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
                ImGui::TableSetupColumn("Title");
                ImGui::TableSetupColumn("Uploader");
                ImGui::TableSetupColumn("Duration");
                ImGui::TableSetupColumn("");
                ImGui::TableHeadersRow();
                for (const RemoteTrack& track : snap.results) {
                    ImGui::PushID(track.id.c_str());
                    const Download* download = nullptr;
                    for (const Download& d : snap.downloads) {
                        if (d.track.id == track.id) download = &d;
                    }
                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(track.title.c_str());
                    ImGui::TableNextColumn();
                    ImGui::TextUnformatted(track.uploader.c_str());
                    ImGui::TableNextColumn();
                    int minutes, seconds;
                    splitTime(track.duration, minutes, seconds);
                    ImGui::Text("%d:%02d", minutes, seconds);
                    ImGui::TableNextColumn();
                    if (download == nullptr) {
                        if (ImGui::SmallButton("Download MP3")) soundcloud.startDownload(track);
                    }
                    else if (download->state == DownloadState::Downloading) {
                        ImGui::TextDisabled("Downloading...");
                    }
                    else if (download->state == DownloadState::Done) {
                        ImGui::Text("Done");
                    }
                    else {
                        if (ImGui::SmallButton("Failed - retry")) soundcloud.startDownload(track);
                    }
                    ImGui::PopID();
                }
                ImGui::EndTable();
            }
        }
        ImGui::End();
    }

    ImGui::Render();
    SDL_SetRenderDrawColor(renderer, 0x0d, 0x08, 0x09, 255);
    SDL_RenderClear(renderer);
    ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
    SDL_RenderPresent(renderer);
}

UI::~UI() {
    icons.clear();
    covers.clear();
    if (imguiReady) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (sdlReady) SDL_Quit();
}