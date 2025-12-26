// global_channels_ui.h
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "TigerEngine/Technique/Tfx/global_channels.h"
#include "TigerEngine/Technique/Tfx/global_channel_usage.h"
#include "Renderer/Graphics/ImGui/imgui.h"
#include <array>
#include <vector>
#include <numeric>
#include <algorithm>
#include <cctype>

inline bool EditChannelWidget(int idx, GlobalChannel& g)
{
    bool changed = false;


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


inline bool ShowGlobalChannelsEditor(std::array<GlobalChannel, 256>& chans,
    ExternStorage& externs,
    bool autoPublish = true)
{
    bool anyChanged = false;

    
    static char filter[64] = "";
    static bool showUnnamed = false;
    static bool onlyChanged = false;
    ImGui::SeparatorText("Global Channels");
    ImGui::TextDisabled("Per-frame uses shown; counts reset each frame.");

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
    if (ImGui::BeginTable("gc_table", 4, flags, ImVec2(0, tableHeight)))
    {
        ImGui::TableSetupColumn("Idx", ImGuiTableColumnFlags_WidthFixed, 48.0f);
        ImGui::TableSetupColumn("Uses", ImGuiTableColumnFlags_WidthFixed, 64.0f);
        ImGui::TableSetupColumn("Name / Type", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableHeadersRow();

        static bool sortByUses = true;
        ImGui::SameLine();
        ImGui::Checkbox("sort by uses", &sortByUses);

        std::vector<int> orderIdx(256);
        std::iota(orderIdx.begin(), orderIdx.end(), 0);
        if (sortByUses) {
            std::stable_sort(orderIdx.begin(), orderIdx.end(), [](int a, int b) {
                const uint32_t ua = tfx::g_global_channel_uses[a];
                const uint32_t ub = tfx::g_global_channel_uses[b];
                if (ua != ub) return ua > ub; // desc
                return a < b;
            });
        }

        for (int ii = 0; ii < 256; ++ii)
        {
            const int i = orderIdx[ii];
        {
            const GlobalChannel& g = chans[i];
            const bool named = (g.name && g.name[0] != '\0');

            if (!f.empty())
            {
                bool pass = false;
          
                char ibuf[16]; snprintf(ibuf, sizeof(ibuf), "%d", i);
                std::string il = ibuf;
         
                std::string nl = named ? g.name : "";
                std::transform(nl.begin(), nl.end(), nl.begin(), [](unsigned char c) { return std::tolower(c); });

                pass = (nl.find(f) != std::string::npos) || (il.find(f) != std::string::npos);
                if (!pass) continue;
            }

            if (!named && !showUnnamed) continue;

            ImGui::TableNextRow();

     
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%03d", i);

            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%u", tfx::g_global_channel_uses[i]);
            ImGui::TableSetColumnIndex(2);
            const char* typeStr =
                (g.type == ChannelType::Float) ? "Float" :
                (g.type == ChannelType::FloatRanged) ? "FloatRanged" : "Color";
            if (named) ImGui::Text("%s  \xE2\x80\x94  %s", g.name, typeStr); // "name — type"
            else       ImGui::Text("<unnamed>  \xE2\x80\x94  %s", typeStr);

            // col 3: widget
            ImGui::TableSetColumnIndex(3);
            ImGui::PushID(i);
            GlobalChannel tmp = g; // edit copy; write back only if changed
            if (EditChannelWidget(i, tmp))
            {
                chans[i] = tmp;
                anyChanged = true;
            }
            ImGui::PopID();
        }
        }

        ImGui::EndTable();
    }

    if (anyChanged && autoPublish)
    {
        PublishGlobalChannelsToExterns(externs, chans);

        externs.scopes[TfxExtern::Generic].dirty = true;
    }

    return anyChanged;
}
