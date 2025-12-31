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
#include <random>
#include <chrono>

static inline std::string Hex8(uint32_t x) {
    char b[16]; std::snprintf(b, sizeof(b), "%08X", x);
    return b;
}

static int  g_selectedEntityIndex = -1;
static char g_newChannelHex[16] = { 0 };

inline void SetAllEntityChannelsToZero(std::vector<RenderEntity>& entities)
{
    for (auto& e : entities) {
        for (auto& kv : e.channels) {
            kv.second = Vec4(0, 0, 0, 0);
        }
    }
}

static bool  g_partyModeEnabled = false;
static float g_partyModeSpeed = 0.35f;
static float g_partyPhase = 0.0f;
static uint32_t g_partySessionSeed = 0;
static float g_partyAmplitude = 5.0f;

static inline uint32_t Hash32(uint32_t x)
{
    x ^= x >> 16;
    x *= 0x7feb352dU;
    x ^= x >> 15;
    x *= 0x846ca68bU;
    x ^= x >> 16;
    return x;
}

static inline float U01FromU32(uint32_t x)
{
    return (float)(x & 0x00FFFFFFu) / (float)0x01000000u;
}

static inline float SNormFromU32(uint32_t x)
{
    return (U01FromU32(x) * 2.0f) - 1.0f;
}

static inline uint32_t MakeSessionSeed()
{
    std::random_device rd;
    uint32_t a = (uint32_t)rd();
    uint32_t b = (uint32_t)rd();
    uint64_t t = (uint64_t)std::chrono::high_resolution_clock::now().time_since_epoch().count();
    uint32_t c = (uint32_t)(t ^ (t >> 32));
    uint32_t s = a ^ (b * 0x9E3779B9u) ^ (c * 0x85EBCA6Bu);
    return (s == 0) ? 1u : s;
}

static inline void EnsurePartySessionSeed()
{
    if (g_partySessionSeed == 0) {
        g_partySessionSeed = MakeSessionSeed();
    }
}

static inline uint32_t PartySeedFor(uint32_t entId, uint32_t chanHash)
{
    EnsurePartySessionSeed();
    uint32_t s = g_partySessionSeed;
    s ^= Hash32(entId + 0x9e3779b9u);
    s ^= Hash32(chanHash + 0x7f4a7c15u);
    return Hash32(s);
}

// ---- scalar "party" value (old behavior), used internally per component ----
static inline float PartyScalarFor(uint32_t entId, uint32_t chanHash, uint32_t componentSalt, float phase)
{
    const uint32_t base0 = PartySeedFor(entId, chanHash);
    const uint32_t base = Hash32(base0 ^ componentSalt); // make x/y/z/w different

    const float twoPi = 6.28318530717958647692f;

    const float ph1 = U01FromU32(Hash32(base ^ 0xA1u)) * twoPi;
    const float ph2 = U01FromU32(Hash32(base ^ 0xB2u)) * twoPi;
    const float ph3 = U01FromU32(Hash32(base ^ 0xC3u)) * twoPi;

    const float sp1 = 0.25f + 2.50f * U01FromU32(Hash32(base ^ 0xD4u)); // [0.25, 2.75]
    const float sp2 = 0.05f + 1.25f * U01FromU32(Hash32(base ^ 0xE5u)); // [0.05, 1.30]
    const float sp3 = 0.02f + 0.60f * U01FromU32(Hash32(base ^ 0xF6u)); // [0.02, 0.62]

    const float a1 = 0.35f + 0.65f * U01FromU32(Hash32(base ^ 0x11u)); // [0.35, 1.0]
    const float a2 = 0.00f + 0.60f * U01FromU32(Hash32(base ^ 0x22u)); // [0.0, 0.6]
    const float a3 = 0.00f + 0.40f * U01FromU32(Hash32(base ^ 0x33u)); // [0.0, 0.4]

    const float bias = 0.20f * SNormFromU32(Hash32(base ^ 0x44u));       // [-0.2, 0.2]
    const float wobPh = U01FromU32(Hash32(base ^ 0x55u)) * twoPi;
    const float wobSp = 0.01f + 0.08f * U01FromU32(Hash32(base ^ 0x66u)); // very slow
    const float wobAmp = 0.05f + 0.20f * U01FromU32(Hash32(base ^ 0x77u)); // small

    const float wobble = wobAmp * std::sinf(phase * wobSp + wobPh);

    float v = 0.0f;
    v += a1 * std::sinf(phase * sp1 + ph1);
    v += a2 * std::sinf(phase * sp2 + ph2);
    v += a3 * std::sinf(phase * sp3 + ph3);
    v += bias;
    v += wobble;

    const float denom = std::max(0.001f, (a1 + a2 + a3 + std::fabs(bias) + std::fabs(wobAmp)));
    v /= denom;

    if (v < -1.0f) v = -1.0f;
    if (v > 1.0f) v = 1.0f;

    return v * g_partyAmplitude;
}

// ---- Vec4 party value ----
static inline Vec4 PartyValueFor(uint32_t entId, uint32_t chanHash, float phase)
{
    // unique salt per component
    float x = PartyScalarFor(entId, chanHash, 0x100u, phase);
    float y = PartyScalarFor(entId, chanHash, 0x200u, phase);
    float z = PartyScalarFor(entId, chanHash, 0x300u, phase);
    float w = PartyScalarFor(entId, chanHash, 0x400u, phase);
    return Vec4(x, y, z, w);
}

inline void ApplyPartyModeToAllEntities(std::vector<RenderEntity>& entities, float deltaSeconds)
{
    EnsurePartySessionSeed();

    const float omega = g_partyModeSpeed * 2.0f * 3.14159265358979323846f;
    g_partyPhase += omega * std::max(deltaSeconds, 0.0f);

    for (auto& e : entities) {
        const uint32_t eid = (uint32_t)e.id;
        for (auto& kv : e.channels) {
			if (kv.first == 0xA7A7FE43) continue;
            kv.second = PartyValueFor(eid, kv.first, g_partyPhase);
        }
    }
}

inline void ShowEntityChannelEditorUI(std::vector<RenderEntity>& entities, XMFLOAT3 camPos)
{
    if (!ImGui::Begin("Entity Channel Editor")) { ImGui::End(); return; }

    // Party mode runs every frame while enabled.
    const float dt = ImGui::GetIO().DeltaTime;
    if (g_partyModeEnabled) {
        ApplyPartyModeToAllEntities(entities, dt);
    }

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

    // Global controls
    if (ImGui::Button("Zero ALL channels (all entities)")) {
        SetAllEntityChannelsToZero(entities);
    }
    ImGui::SameLine();
    if (ImGui::Checkbox("Party Mode", &g_partyModeEnabled) && g_partyModeEnabled) {
        g_partySessionSeed = MakeSessionSeed();
    }

    ImGui::SameLine();
    if (ImGui::Button("Reseed")) {
        g_partySessionSeed = MakeSessionSeed();
    }

    ImGui::SetNextItemWidth(260);
    ImGui::DragFloat("Party Speed (cycles/sec)", &g_partyModeSpeed, 0.01f, 0.01f, 10.0f);
    ImGui::SetNextItemWidth(260);
    ImGui::DragFloat("Party Amplitude", &g_partyAmplitude, 0.1f, 0.0f, 50.0f);
    ImGui::Separator();

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

    // Add channel
    ImGui::TextDisabled("Add channel (hex hash):");
    ImGui::SetNextItemWidth(120);
    ImGui::InputText("##newch", g_newChannelHex, sizeof(g_newChannelHex));
    ImGui::SameLine();

    static Vec4 newVal = Vec4(0, 0, 0, 0);
    ImGui::SetNextItemWidth(260);
    ImGui::DragFloat4("Value##newchval", &newVal.x, 0.01f);

    if (ImGui::Button("Add/Update")) {
        char* endp = nullptr;
        const uint32_t h = (uint32_t)strtoul(g_newChannelHex, &endp, 16);
        if (endp != g_newChannelHex) chan[h] = newVal;
    }

    ImGui::Separator();

    // Keys sorted
    std::vector<uint32_t> keys; keys.reserve(chan.size());
    for (auto& kv : chan) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    keys.erase(std::unique(keys.begin(), keys.end()), keys.end());

    if (ImGui::BeginTable("chans", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("Hash");
        ImGui::TableSetupColumn("Value (Vec4)");
        ImGui::TableSetupColumn("Ops");
        ImGui::TableHeadersRow();

        ImGui::PushID((int)ent.id);

        for (uint32_t h : keys) {
            ImGui::TableNextRow();

            ImGui::TableSetColumnIndex(0);
            char hex[16]; std::snprintf(hex, sizeof(hex), "%08X", h);
            ImGui::TextUnformatted(hex);

            ImGui::TableSetColumnIndex(1);
            Vec4& v = chan[h];

            ImGui::PushID((int)h);
            ImGui::DragFloat4("##v", &v.x, 0.01f);

            ImGui::TableSetColumnIndex(2);
            if (ImGui::SmallButton("Zero")) v = Vec4(0, 0, 0, 0);
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
