// global_channels_ui.h
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "TigerEngine/Technique/Tfx/global_channels.h"
#include "Renderer/Graphics/ImGui/imgui.h"
#include <array>
#include <algorithm>
#include <cctype>

inline bool EditChannelWidget(int idx, GlobalChannel& g)
{
    bool changed = false;

    // label is hidden (##) so the row's visible label can be clean
    char idbuf[32];
    snprintf(idbuf, sizeof(idbuf), "##ch%03d", idx);

    switch (g.type)
    {
    case ChannelType::Float:
    {
        float v = g.value.x;
        if (ImGui::DragFloat(idbuf, &v, 0.01f)) { g.value.x = v; changed = true; }
        break;
    }
    case ChannelType::FloatRanged:
    {
        float v = g.value.x;
        if (ImGui::SliderFloat(idbuf, &v, g.minv, g.maxv)) { g.value.x = v; changed = true; }
        break;
    }
    case ChannelType::Color:
    {
        float c[4] = { g.value.x, g.value.y, g.value.z, g.value.w };
        if (ImGui::ColorEdit4(idbuf, c, ImGuiColorEditFlags_Float))
        {
            g.value = Vec4(c[0], c[1], c[2], c[3]);
            changed = true;
        }
        break;
    }
    }
    return changed;
}

// Returns true if any value changed. If autoPublish==true, publishes on change.
inline bool ShowGlobalChannelsEditor(std::array<GlobalChannel, 256>& chans,
    ExternStorage& externs,
    bool autoPublish = true)
{
    bool anyChanged = false;

    // --- Header controls ---
    static char filter[64] = "";
    static bool showUnnamed = false;
    static bool onlyChanged = false; // (not tracked per-row here, but left for future)
    ImGui::SeparatorText("Global Channels");

    ImGui::SetNextItemWidth(240);
    ImGui::InputTextWithHint("##filter", "filter (index or name substring)", filter, IM_ARRAYSIZE(filter));
    ImGui::SameLine();
    ImGui::Checkbox("show unnamed", &showUnnamed);
    ImGui::SameLine();
    if (ImGui::Button("Reset All to Defaults"))
    {
        chans = GetGlobalChannelDefaults();
        anyChanged = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Publish Now"))
    {
        PublishGlobalChannelsToExterns(externs, chans);
        externs.Ensure(nullptr, TfxExtern::Generic); // safe no-op if already created with EnsureAll
        externs.scopes[TfxExtern::Generic].dirty = true; // mark dirty
        anyChanged = true;
    }

    // Lowercase filter once
    std::string f = filter;
    std::transform(f.begin(), f.end(), f.begin(), [](unsigned char c) { return std::tolower(c); });

    // --- Table ---
    ImGuiTableFlags flags = ImGuiTableFlags_SizingStretchProp |
        ImGuiTableFlags_RowBg |
        ImGuiTableFlags_Resizable |
        ImGuiTableFlags_ScrollY |
        ImGuiTableFlags_BordersOuterH;

    const float tableHeight = ImGui::GetTextLineHeightWithSpacing() * 18.0f; // ~18 rows visible
    if (ImGui::BeginTable("gc_table", 3, flags, ImVec2(0, tableHeight)))
    {
        ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Name / Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        for (int i = 0; i < 256; ++i)
        {
            const GlobalChannel& g = chans[i];
            const bool named = (g.name && g.name[0] != '\0');

            // filter by name or index
            if (!f.empty())
            {
                bool pass = false;
                // index match?
                char ibuf[16]; snprintf(ibuf, sizeof(ibuf), "%d", i);
                std::string il = ibuf;
                // name match?
                std::string nl = named ? g.name : "";
                std::transform(nl.begin(), nl.end(), nl.begin(), [](unsigned char c) { return std::tolower(c); });

                pass = (nl.find(f) != std::string::npos) || (il.find(f) != std::string::npos);
                if (!pass) continue;
            }

            if (!named && !showUnnamed) continue;

            ImGui::TableNextRow();

            // col 0: index
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%03d", i);

            // col 1: name + type tag
            ImGui::TableSetColumnIndex(1);
            const char* typeStr =
                (g.type == ChannelType::Float) ? "Float" :
                (g.type == ChannelType::FloatRanged) ? "FloatRanged" : "Color";
            if (named) ImGui::Text("%s  \xE2\x80\x94  %s", g.name, typeStr); // "name — type"
            else       ImGui::Text("<unnamed>  \xE2\x80\x94  %s", typeStr);

            // col 2: widget
            ImGui::TableSetColumnIndex(2);
            ImGui::PushID(i);
            GlobalChannel tmp = g; // edit copy; write back only if changed
            if (EditChannelWidget(i, tmp))
            {
                chans[i] = tmp;
                anyChanged = true;
            }
            ImGui::PopID();
        }

        ImGui::EndTable();
    }

    if (anyChanged && autoPublish)
    {
        PublishGlobalChannelsToExterns(externs, chans);
        // Make sure GPU buffer gets updated this frame.
        // If you already call externs.UploadAll(ctx) later, this is enough:
        // just mark dirty.
        externs.scopes[TfxExtern::Generic].dirty = true;
    }

    return anyChanged;
}
