#pragma once

#include "imgui.h"
#include "misc/freetype/imgui_freetype.h"

#define FONT_FILE "C:/Windows/Fonts/simhei.ttf"
#define FONT_SIZE 18.0f

inline static void LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF(FONT_FILE, FONT_SIZE, nullptr, io.Fonts->GetGlyphRangesChineseFull());
    ImGuiFreeType::BuildFontAtlas(io.Fonts);
}
