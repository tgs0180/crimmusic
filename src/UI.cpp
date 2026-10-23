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
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <cstddef>
#include "StringUtils.hpp"
#include <string>
#include <cstdlib>
#include <fstream>
#include <sstream>

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

static ImVec4 hexColor(unsigned int rgb, float alpha = 1.0f) {
    float r = ((rgb >> 16) & 0xFF) / 255.0f;
    float g = ((rgb >> 8) & 0xFF) / 255.0f;
    float b = (rgb & 0xFF) / 255.0f;
    return ImVec4(r, g, b, alpha);
}

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

    style.FrameRounding  = 6.0f;
    style.GrabRounding   = 6.0f;
    style.ChildRounding  = 6.0f;
    style.PopupRounding  = 8.0f;
    style.WindowRounding = 8.0f;
}

bool UI::iconButton(const char *iconName, bool active, float size) {
    SDL_Texture* tex = icons.get(iconName);
    if (tex == nullptr) return ImGui::Button(iconName);
    if (size == 0.0f) size = ImGui::GetFontSize();
    ImVec4 tint = active ? hexColor(palette::crimson) : ImVec4(1, 1, 1, 1);
    return ImGui::ImageButton(iconName, (ImTextureID)(intptr_t)tex, ImVec2(size, size), ImVec2(0,0), ImVec2(1,1), ImVec4(0,0,0,0), tint);
}

bool UI::init() {
    if (!SDL_Init(SDL_INIT_VIDEO)) {
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
    SDL_SetRenderVSync(renderer, 1);
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

void UI::drawPlayPause(Player & player, float iconSize){
    if (iconButton(player.isPlaying() ? "pause" : "play", false, iconSize)) {
        player.togglePause();
    }
}

void UI::drawQueue(Player &player, Library &lib, float iconSize) {
    if (iconButton("list-video", false, iconSize)) {
        ImGui::OpenPopup("queue");
    }
    ImGui::SetItemTooltip("Queue");
    ImGui::SetNextWindowPos(ImGui::GetItemRectMin(), ImGuiCond_Appearing, ImVec2(0.5f, 1.0f));
    if (ImGui::BeginPopup("queue")) {
        ImGui::TextDisabled("Up next");
        ImGui::Separator();
        ImGui::BeginChild("Queuelist", ImVec2(500, 400));
        std::vector<std::size_t> next = lib.upcoming(lib.size());
        if (next.empty()) ImGui::Text("It's quiet in here...");
        else {
            float rowHeight = ImGui::GetTextLineHeightWithSpacing() * 2;
            float coverSize = 32.0f;
            ImGuiListClipper clipper;
            clipper.Begin(static_cast<int>(next.size()));
            while (clipper.Step()) {
            for (int k = clipper.DisplayStart; k < clipper.DisplayEnd; k++) {
                const Track& t = lib.song(next[static_cast<std::size_t>(k)]);
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
                        lib.next();
                        x -= 1;
                    }
                    player.play(lib.currentSong());
                }
                ImGui::PopID();
            }
            }
        }
        ImGui::EndChild();
        ImGui::EndPopup();
    }
}

void UI::drawRepeat(Library &lib, float iconSize) {
    RepeatMode repeatmode = lib.repeatMode();
    const char* repeatstr;
    if (repeatmode == RepeatMode::Off) repeatstr = "repeat-off";
    else if (repeatmode == RepeatMode::All) repeatstr = "repeat";
    else if (repeatmode == RepeatMode::One) repeatstr = "repeat-1";

    if (iconButton(repeatstr, repeatmode != RepeatMode::Off, iconSize)) {
        if (lib.repeatMode() == RepeatMode::All) lib.setRepeatMode(RepeatMode::Off);
        else if (lib.repeatMode() == RepeatMode::Off) lib.setRepeatMode(RepeatMode::One);
        else if (lib.repeatMode() == RepeatMode::One) lib.setRepeatMode(RepeatMode::All);
    }
}

void UI::drawShuffle(Library &lib, float iconSize) {
    if (iconButton("shuffle", lib.shuffled(), iconSize)) {
        lib.setShuffle(!lib.shuffled());
    }
}

void UI::drawTimeSlider(Player &player, float width) {
    float sliderY = ImGui::GetCursorPosY();
    ImGui::SetNextItemWidth(width);
    float pos = player.position();
    if (ImGui::SliderFloat("##position", &pos, 0.0f, player.length(), "")) {
        player.seek(pos);
    }
    ImGui::SameLine();
    ImGui::SetCursorPosY(sliderY);
    ImGui::AlignTextToFramePadding();
    int posMin, posSec, lenMin, lenSec;
    splitTime(pos, posMin, posSec);
    splitTime(player.length(), lenMin, lenSec);
    ImGui::Text("%d:%02d / %d:%02d", posMin, posSec, lenMin, lenSec);
}

void UI::drawVolume(Player &player, float width) {
    ImGui::SetNextItemWidth(width);
    float vol = player.volume() * 100;
    if (ImGui::SliderFloat("##volume", &vol, 0.0f, 100.0f, "%.0f%%")) {
        player.setVolume(vol/100.0f);
    }
}

void UI::drawLyrics(Player &player, Library &library, float x, float y, float width, float height) {
    const Track& track = library.song(library.currentIndex());
    const std::vector<LyricLine>& lines = lyrics.linesFor(track.file, track.artist, track.title);

    if (track.file != lyricsSong) {
        lyricsSong = track.file;
        shownLine = -1;
        previousLine = -1;
        lineSlide = 1.0f;
    }

    int current = -1;
    float position = player.position();
    for (std::size_t i = 0; i < lines.size(); i++) {
        if (lines[i].time > position) break;
        current = static_cast<int>(i);
    }

    if (current != shownLine) {
        previousLine = shownLine;
        shownLine = current;
        lineSlide = 0.0f;
    }
    lineSlide = std::clamp(lineSlide + ImGui::GetIO().DeltaTime / 0.3f, 0.0f, 1.0f);
    float t = 1.0f - lineSlide;
    float eased = 1.0f - t * t * t;

    ImVec2 windowPos = ImGui::GetWindowPos();
    ImVec2 areaMin(windowPos.x + x, windowPos.y + y);
    ImVec2 areaMax(areaMin.x + width, areaMin.y + height);
    float lineHeight = ImGui::GetTextLineHeight();
    float centreY = areaMin.y + (height - lineHeight) / 2.0f;

    ImGuiCol textColour = lyrics.isTranscribed(track.file) ? ImGuiCol_TextDisabled : ImGuiCol_Text;
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(areaMin, areaMax, true);

    auto drawLine = [&](int index, float lineY, float alpha) {
        if (index < 0 || static_cast<std::size_t>(index) >= lines.size()) return;
        std::string text = fitText(lines[static_cast<std::size_t>(index)].text, width);
        if (text.empty()) return;
        float textWidth = ImGui::CalcTextSize(text.c_str()).x;
        drawList->AddText(ImVec2(areaMin.x + (width - textWidth) / 2.0f, lineY), ImGui::GetColorU32(textColour, alpha), text.c_str());
    };
    drawLine(previousLine, centreY - eased * height, 1.0f - eased);
    drawLine(shownLine, centreY + (1.0f - eased) * height, eased);

    drawList->PopClipRect();
}

void UI::draw(Player &player, Library &library, SoundCloud &soundcloud) {
    covers.update(renderer);
    bigCovers.update(renderer);
    lyrics.update();
    ImGui_ImplSDLRenderer3_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    ImGui::NewFrame();

    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
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
        if (ImGui::MenuItem("Make lyrics")) {
            showMakeLyrics = true;
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
        float tile = static_cast<float>(covers.size());
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
        if (ImGui::IsItemClicked()) {NowPlayingTarget = true; JustOpened = true;}
    }
    if (iconButton("skip-back") && !library.empty()) {
        library.previous();
        player.play(library.currentSong());
    }

    ImGui::SameLine();
    drawPlayPause(player);

    ImGui::SameLine();
    if (iconButton("skip-forward") && !library.empty()) {
        library.next();
        player.play(library.currentSong());
    }

    ImGui::SameLine();
    float rightSide = ImGui::CalcTextSize("00:00 / 00:00").x + 150 + ImGui::GetStyle().ItemSpacing.x * 5 + iconButtonWidth()*3;
    drawTimeSlider(player, -rightSide);
    
    ImGui::SameLine();
    drawRepeat(library);

    ImGui::SameLine();
    drawShuffle(library);

    ImGui::SameLine();
    drawQueue(player, library);
    

    ImGui::SameLine();
    drawVolume(player, 150);
    
    ImGui::End();

    float step = ImGui::GetIO().DeltaTime / 0.25f;
    nowPlayingSlide += NowPlayingTarget ? step : -step;
    nowPlayingSlide = std::clamp(nowPlayingSlide, 0.0f, 1.0f);

    if (nowPlayingSlide > 0.0f) {
        if (JustOpened) {
            ImGui::SetNextWindowFocus();
            JustOpened = false;
        }
        float t = 1.0f - nowPlayingSlide;
        float eased = 1.0f - t * t * t;
        float offset = (1.0f - eased) * viewport->WorkSize.y;
        ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x, viewport->WorkPos.y + offset));
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings;
        ImGui::Begin("nowplaying", nullptr, flags);

        const ImGuiStyle& style = ImGui::GetStyle();
        float left = ImGui::GetCursorPosX();
        float width = ImGui::GetContentRegionAvail().x;
        auto centerX = [&](float itemWidth) { ImGui::SetCursorPosX(left + (width - itemWidth) / 2.0f); };

        if (ImGui::Button("X")) NowPlayingTarget = false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) NowPlayingTarget = false;

        if (library.empty()) {
            ImGui::Text("Nothing is playing");
        }
        else {
            const Track& t = library.song(library.currentIndex());

            float closeButtonRoom = (ImGui::GetItemRectMax().x - ImGui::GetWindowPos().x) * 2;
            std::string title = fitText(t.title, width - closeButtonRoom);
            std::string artist = fitText(t.artist, width - closeButtonRoom);
            ImGui::SameLine();
            centerX(ImGui::CalcTextSize(title.c_str()).x);
            ImGui::TextUnformatted(title.c_str());
            centerX(ImGui::CalcTextSize(artist.c_str()).x);
            ImGui::TextDisabled("%s", artist.c_str());

            float height = ImGui::GetContentRegionAvail().y;

            const float bigIcon = 48.0f;
            const float smallIcon = 32.0f;
            float row1Height = bigIcon + style.FramePadding.y * 2;
            float row2Height = smallIcon + style.FramePadding.y * 2;

            float gap = 20.0f;
            float reserved = gap + gap + row1Height + style.ItemSpacing.y + row2Height + gap;
            float coverSize = std::max(64.0f, std::min(width * 0.6f, height - reserved));

            float stackHeight = reserved - gap + coverSize;
            ImGui::Dummy(ImVec2(0, std::max(0.0f, (height - stackHeight) / 2.0f)));

            ImGui::Dummy(ImVec2(0, gap));
            centerX(coverSize);
            bigCovers.request(library.currentSong());
            SDL_Texture* tex = bigCovers.get(library.currentSong());
            if (tex) ImGui::Image((ImTextureID)(intptr_t)tex, ImVec2(coverSize, coverSize));
            else ImGui::Dummy(ImVec2(coverSize, coverSize));
            ImGui::Dummy(ImVec2(0, gap));

            float rowTop = ImGui::GetCursorPosY();
            float bigButtonWidth = bigIcon + style.FramePadding.x * 2;
            float margin = width * 0.08f;

            ImGui::SetCursorPosX(left + margin);
            if (iconButton("skip-back", false, bigIcon)) {
                library.previous();
                player.play(library.currentSong());
            }

            float timeTextWidth = ImGui::CalcTextSize("00:00 / 00:00").x;
            float sliderWidth = width - 2 * (margin + bigButtonWidth + style.ItemSpacing.x * 2) - timeTextWidth - style.ItemSpacing.x;
            sliderWidth = std::max(50.0f, sliderWidth);
            float blockWidth = sliderWidth + style.ItemSpacing.x + timeTextWidth;
            ImGui::SameLine();
            centerX(blockWidth);
            ImGui::SetCursorPosY(rowTop + (row1Height - ImGui::GetFrameHeight()) / 2.0f);
            drawTimeSlider(player, sliderWidth);

            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(left + width - margin - bigButtonWidth, rowTop));
            if (iconButton("skip-forward", false, bigIcon)) {
                library.next();
                player.play(library.currentSong());
            }

            float row2Top = rowTop + row1Height + style.ItemSpacing.y;
            float smallButtonWidth = smallIcon + style.FramePadding.x * 2;

            ImGui::SetCursorPos(ImVec2(left + margin, row2Top));
            drawPlayPause(player, smallIcon);
            ImGui::SameLine();
            drawRepeat(library, smallIcon);
            ImGui::SameLine();
            drawShuffle(library, smallIcon);

            float quarter = width / 4.0f;
            float volumeWidth = std::max(60.0f, std::min(150.0f, quarter - margin - smallButtonWidth - style.ItemSpacing.x));
            float rightGroupWidth = smallButtonWidth + style.ItemSpacing.x + volumeWidth;
            ImGui::SameLine();
            ImGui::SetCursorPos(ImVec2(left + width - margin - rightGroupWidth, row2Top));
            drawQueue(player, library, smallIcon);
            ImGui::SameLine();
            ImGui::SetCursorPosY(row2Top + (row2Height - ImGui::GetFrameHeight()) / 2.0f);
            drawVolume(player, volumeWidth);

            drawLyrics(player, library, left + quarter, row2Top, width / 2.0f, row2Height);
        }

        ImGui::End();
    }

    if (showMakeLyrics) {
        drawMakeLyrics(library);
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
    bigCovers.clear();
    lyrics.clear();
    if (imguiReady) {
        ImGui_ImplSDLRenderer3_Shutdown();
        ImGui_ImplSDL3_Shutdown();
        ImGui::DestroyContext();
    }
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window) SDL_DestroyWindow(window);
    if (sdlReady) SDL_Quit();
}

void UI::onMp3Chosen(void *userdata, const char *const *filelist, int filter) {
    if (filelist == nullptr || filelist[0] == nullptr) return;
    UI* ui = static_cast<UI*>(userdata);
    std::lock_guard<std::mutex> lock(ui->folderMutex);
    ui->pendingMp3 = filelist[0];
}

void UI::onTxtChosen(void *userdata, const char *const *filelist, int filter) {
    if (filelist == nullptr || filelist[0] == nullptr) return;
    UI* ui = static_cast<UI*>(userdata);
    std::lock_guard<std::mutex> lock(ui->folderMutex);
    ui->pendingTxt = filelist[0];
}

void UI::saveMadeLyrics(const MakerResult &result) {
    std::filesystem::path target = result.song;
    target.replace_extension(".lrc");
    std::error_code error;
    if (std::filesystem::exists(target, error)) {
        const char* home = std::getenv("HOME");
        std::filesystem::path backup = std::filesystem::path(home ? home : ".") / ".cache" / "crimmusic" / "lrc-backup" / "manual";
        std::filesystem::create_directories(backup, error);
        std::filesystem::copy_file(target, backup / target.filename(), std::filesystem::copy_options::overwrite_existing, error);
    }
    std::ofstream out(target);
    if (!out) {
        makeStatus = "Could not write " + target.filename().string();
        return;
    }
    out << result.lrc;
    out.close();
    lyrics.forget(result.song);
    makeStatus = "Saved " + target.filename().string();
}

void UI::drawMakeLyrics(Library &library) {
    {
        std::string mp3, txt;
        {
            std::lock_guard<std::mutex> lock(folderMutex);
            mp3.swap(pendingMp3);
            txt.swap(pendingTxt);
        }
        if (!mp3.empty()) {
            makeSong = mp3;
            makeStatus.clear();
        }
        if (!txt.empty()) {
            std::ifstream file(txt);
            std::stringstream contents;
            contents << file.rdbuf();
            std::string text = contents.str();
            if (text.size() >= sizeof(makeLyricsText)) text.resize(sizeof(makeLyricsText) - 1);
            std::copy(text.begin(), text.end(), makeLyricsText);
            makeLyricsText[text.size()] = '\0';
        }
    }

    ImGui::SetNextWindowSize(ImVec2(640, 620), ImGuiCond_Appearing);
    if (!ImGui::Begin("Make lyrics", &showMakeLyrics)) {
        ImGui::End();
        return;
    }

    static const SDL_DialogFileFilter mp3Filter[] = { { "MP3 audio", "mp3" } };
    static const SDL_DialogFileFilter txtFilter[] = { { "Text files", "txt;lrc" }, { "All files", "*" } };
    if (!library.empty()) dialogLocation = library.currentSong().parent_path().string();

    ImGui::TextDisabled("Song");
    ImGui::TextWrapped("%s", makeSong.empty() ? "(none chosen)" : makeSong.filename().string().c_str());
    if (ImGui::Button("Choose mp3...")) {
        SDL_ShowOpenFileDialog(onMp3Chosen, this, window, mp3Filter, 1, dialogLocation.empty() ? nullptr : dialogLocation.c_str(), false);
    }
    ImGui::SameLine();
    ImGui::BeginDisabled(library.empty());
    if (ImGui::Button("Use current song")) {
        makeSong = library.currentSong();
        makeStatus.clear();
    }
    ImGui::EndDisabled();

    ImGui::Separator();
    ImGui::TextDisabled("Lyrics, one line per lyric line");
    if (ImGui::Button("Load text file...")) {
        SDL_ShowOpenFileDialog(onTxtChosen, this, window, txtFilter, 2, dialogLocation.empty() ? nullptr : dialogLocation.c_str(), false);
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear")) makeLyricsText[0] = '\0';
    ImGui::InputTextMultiline("##makelyrics", makeLyricsText, sizeof(makeLyricsText), ImVec2(-1, 200));

    ImGui::Separator();
    MakerResult result = maker.snapshot();
    bool running = result.state == MakerState::Running;
    bool available = LyricsMaker::available();
    ImGui::BeginDisabled(running || makeSong.empty() || makeLyricsText[0] == '\0' || !available);
    if (ImGui::Button("Make timed lyrics")) {
        makeStatus.clear();
        maker.start(makeSong, makeLyricsText);
    }
    ImGui::EndDisabled();

    if (!available) {
        ImGui::TextColored(hexColor(palette::bright), "The lyrics tool is not set up. Run tools/lyrics/setup.sh first.");
    }
    if (running) {
        int dots = static_cast<int>(ImGui::GetTime() * 2.0) % 4;
        ImGui::Text("Listening to the song and placing the lines%s", std::string(dots, '.').c_str());
        ImGui::TextDisabled("This takes about 10 seconds to a minute.");
    }
    else if (result.state == MakerState::Failed) {
        ImGui::TextColored(hexColor(palette::bright), "%s", result.message.c_str());
    }
    else if (result.state == MakerState::Done) {
        ImGui::TextWrapped("%s", result.song.filename().string().c_str());
        if (result.matches) {
            ImGui::Text("%d lines timed. %.0f%% of the lyric words were heard in the song.", result.lines, result.words * 100.0f);
        }
        else {
            ImGui::TextColored(hexColor(palette::bright), "Warning: only %.0f%% of these words are heard in the song.", result.words * 100.0f);
            ImGui::TextWrapped("These lyrics probably belong to another song, or the song is very hard to understand. Check the preview before saving.");
        }
        if (result.weakLines > 0) {
            ImGui::TextDisabled("%d lines could not be heard clearly, their times are estimated.", result.weakLines);
        }
        if (!result.dropped.empty()) {
            ImGui::TextDisabled("%d repeated lines were left out because they are not sung in this version.", static_cast<int>(result.dropped.size()));
        }

        std::vector<LyricLine> preview = Lyrics::parse(result.lrc);
        ImGui::BeginChild("makepreview", ImVec2(0, 150), ImGuiChildFlags_Borders);
        for (const LyricLine& line : preview) {
            int minutes, seconds;
            splitTime(line.time, minutes, seconds);
            ImGui::TextDisabled("%d:%02d", minutes, seconds);
            ImGui::SameLine();
            ImGui::TextUnformatted(line.text.c_str());
        }
        ImGui::EndChild();

        if (ImGui::Button("Save as .lrc next to the song")) {
            saveMadeLyrics(result);
        }
    }
    if (!makeStatus.empty()) {
        ImGui::SameLine();
        ImGui::TextDisabled("%s", makeStatus.c_str());
    }

    ImGui::End();
}
