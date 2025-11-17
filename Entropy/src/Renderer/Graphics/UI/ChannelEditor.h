#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"
#include "Renderer/Loaders/RenderEntity.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>
#include <cmath>

static inline std::string Hex8(uint32_t x) {
    char b[16]; std::snprintf(b, sizeof(b), "%08X", x);
    return b;
}

static int  g_selectedEntityIndex = -1;
static char g_newChannelHex[16] = { 0 };

inline void ShowEntityChannelEditorUI(std::vector<RenderEntity>& entities, XMFLOAT3 camPos)
{
    if (!ImGui::Begin("Entity Channel Editor")) { ImGui::End(); return; }

    struct Row { int idx; float dist2; };
    std::vector<Row> withChannels;
    withChannels.reserve(entities.size());

    for (int i = 0; i < (int)entities.size(); ++i) {
        const auto& e = entities[i];
        if (!e.channels.empty()) {
            const float dx = e.pos.x - camPos.x;
            const float dy = e.pos.y - camPos.y;
            const float dz = e.pos.z - camPos.z;
            withChannels.push_back({ i, dx * dx + dy * dy + dz * dz });
        }
    }

    std::sort(withChannels.begin(), withChannels.end(),
        [](const Row& a, const Row& b) { return a.dist2 < b.dist2; });

    ImGui::BeginChild("left", ImVec2(300, 0), true);
    if (withChannels.empty()) {
        ImGui::TextDisabled("No entities with channels.");
    }
    else {
        for (const Row& r : withChannels) {
            const auto& e = entities[r.idx];
            const bool selected = (g_selectedEntityIndex == r.idx);
            const float dist = std::sqrt(std::max(r.dist2, 0.0f));

            // Show:  [#]  id  (dist)
            char label[128];
            std::snprintf(label, sizeof(label),
                "%4d  id:%08X  (%.1f)", r.idx, (uint32_t)e.id, dist);

            if (ImGui::Selectable(label, selected)) {
                g_selectedEntityIndex = r.idx;  
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

   
    ImGui::BeginChild("right", ImVec2(0, 0), true);


    RenderEntity* entPtr = nullptr;
    if (g_selectedEntityIndex >= 0 &&
        g_selectedEntityIndex < (int)entities.size())
    {
        entPtr = &entities[g_selectedEntityIndex];
    }

    if (!entPtr) {
        ImGui::TextDisabled("Select an entity with channels on the left.");
        ImGui::EndChild();
        ImGui::End();
        return;
    }

    auto& ent = *entPtr;
    auto& chan = ent.channels;

    ImGui::Text("Entity id: %08X", (uint32_t)ent.id);
    ImGui::Separator();

    ImGui::TextDisabled("Add channel (hex hash):");
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("##newch", g_newChannelHex, sizeof(g_newChannelHex));
    ImGui::SameLine();
    static float newVal = 0.0f;
    ImGui::SetNextItemWidth(120);
    ImGui::DragFloat("Value##newchval", &newVal, 0.01f);
    ImGui::SameLine();
    if (ImGui::Button("Add/Update")) {
        char* endp = nullptr;
        const uint32_t h = (uint32_t)strtoul(g_newChannelHex, &endp, 16);
        if (endp != g_newChannelHex) chan[h] = newVal;
    }

    ImGui::Separator();
    std::vector<uint32_t> keys; keys.reserve(chan.size());
    for (auto& kv : chan) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    if (ImGui::BeginTable("chans", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Hash");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Ops");
        ImGui::TableHeadersRow();

        ImGui::PushID((int)ent.id);

        for (uint32_t h : keys) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            char hex[16]; std::snprintf(hex, sizeof(hex), "%08X", h);
            ImGui::TextUnformatted(hex);

            ImGui::TableSetColumnIndex(1);
            float& v = chan[h];

            ImGui::PushID((int)h);
            ImGui::DragFloat("##v", &v, 0.01f);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("Zero")) v = 0.0f;
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) chan.erase(h);
            ImGui::PopID();
        }

        ImGui::PopID();
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
}
