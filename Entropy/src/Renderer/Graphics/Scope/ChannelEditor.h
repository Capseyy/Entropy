// EntityChannelEditorUI.h (updated)
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"
#include "Renderer/Loaders/RenderEntity.h"
#include <unordered_map>
#include <vector>
#include <string>
#include <cstdio>
#include <algorithm>

static inline std::string Hex8(uint32_t x) {
    char b[16]; std::snprintf(b, sizeof(b), "%08X", x);
    return b;
}

// Track selection by entity id (0 means none)
static uint32_t g_selectedEntityId = 0;
static char     g_newChannelHex[16] = { 0 };

inline void ShowEntityChannelEditorUI(std::vector<RenderEntity>& entities, XMFLOAT3 camPos)
{
    if (!ImGui::Begin("Entity Channel Editor")) { ImGui::End(); return; }

    // Build a list of (index, distance^2) for entities that ALREADY have channels
    struct Row { int idx; float dist2; };
    std::vector<Row> withChannels;
    withChannels.reserve(entities.size());
    for (int i = 0; i < (int)entities.size(); ++i) {
        const auto& e = entities[i];
        if (!e.channels.empty()) {
            // distance^2 to camera
            const float dx = e.pos.x - camPos.x;
            const float dy = e.pos.y - camPos.y;
            const float dz = e.pos.z - camPos.z;
            withChannels.push_back({ i, dx * dx + dy * dy + dz * dz });
        }
    }

    // Sort by distance^2 (ascending == closest first)
    std::sort(withChannels.begin(), withChannels.end(),
        [](const Row& a, const Row& b) { return a.dist2 < b.dist2; });

    // -------- Left: entity list (only those with channels), sorted by distance --------
    ImGui::BeginChild("left", ImVec2(300, 0), true);
    if (withChannels.empty()) {
        ImGui::TextDisabled("No entities with channels.");
    }
    else {
        for (const Row& r : withChannels) {
            const auto& e = entities[r.idx];
            const bool selected = (g_selectedEntityId == (uint32_t)e.id);
            const float dist = std::sqrt(std::max(r.dist2, 0.0f));
            // Show:  [#]  id  (dist)
            char label[128];
            std::snprintf(label, sizeof(label), "%4d  id:%08X  (%.1f)", r.idx, (uint32_t)e.id, dist);
            if (ImGui::Selectable(label, selected)) {
                g_selectedEntityId = (uint32_t)e.id;
            }
        }
    }
    ImGui::EndChild();

    ImGui::SameLine();

    // -------- Right: channels for the selected entity --------
    ImGui::BeginChild("right", ImVec2(0, 0), true);

    // Find selected entity by id (selection persists after sort)
    RenderEntity* entPtr = nullptr;
    if (g_selectedEntityId != 0) {
        for (auto& e : entities) {
            if ((uint32_t)e.id == g_selectedEntityId) { entPtr = &e; break; }
        }
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

    // Show/edit current channels (sorted by key for stable UI)
    std::vector<uint32_t> keys; keys.reserve(chan.size());
    for (auto& kv : chan) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    if (ImGui::BeginTable("chans", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Hash");
        ImGui::TableSetupColumn("Value");
        ImGui::TableSetupColumn("Ops");
        ImGui::TableHeadersRow();

        for (uint32_t h : keys) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            char hex[16]; std::snprintf(hex, sizeof(hex), "%08X", h);
            ImGui::TextUnformatted(hex);

            ImGui::TableSetColumnIndex(1);
            float v = chan[h];
            if (ImGui::DragFloat((std::string("##v") + hex).c_str(), &v, 0.01f)) chan[h] = v;

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton((std::string("Zero##") + hex).c_str())) chan[h] = 0.0f;
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Remove##") + hex).c_str())) chan.erase(h);
        }
        ImGui::EndTable();
    }

    ImGui::EndChild();
    ImGui::End();
}
