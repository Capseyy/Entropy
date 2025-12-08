// FrameGlobalLightingExternUI.h
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"


// ==================== Frame ====================
namespace FrameOff {
    // Offsets we actively edit in UI (stay the same)
    constexpr size_t kGameTime = 0x00; // float
    constexpr size_t kRenderTime = 0x04; // float
    constexpr size_t kUnk0C = 0x0C; // float
    constexpr size_t kUnk10 = 0x10; // float (default 0.50)
    constexpr size_t kDeltaGameTime = 0x14; // float
    constexpr size_t kExposureTime = 0x18; // float
    constexpr size_t kExposureScale = 0x1C; // float

    // A known non-zero default that lives beyond 0x20 (so we seed it when expanding):
    constexpr size_t kUnk1C0 = 0x1C0; // Vec4 default (1,1,0,1)
}

inline void EnsureFrameCapacity(ExternStorage& ex)
{
    // If frame scope is missing or empty, seed it with full FrameExtern defaults.
    auto it = ex.scopes.find(TfxExtern::Frame);
    if (it == ex.scopes.end() || it->second.cpu.empty()) {
        ex.set(TfxExtern::Frame, FrameExtern{}); // alloc full 0x1F8 and default values
        return;
    }

    auto& scope = it->second;

    // If existing buffer is smaller (e.g. old 0x20 UI version), grow it to full size.
    const size_t want = sizeof(FrameExtern); // 0x1F8
    if (scope.cpu.size() < want) {
        const size_t oldSize = scope.cpu.size();
        scope.cpu.resize(want, 0u);
        scope.dirty = true;

        // Preserve existing front fields; also seed known non-zero defaults in the new tail.
        // Only write defaults if the newly added region covers them.
        if (oldSize <= FrameOff::kUnk1C0) {
            const Vec4 def_1C0(1.0f, 1.0f, 0.0f, 1.0f);
            ex.MemcpyScope(TfxExtern::Frame, FrameOff::kUnk1C0, &def_1C0, sizeof(def_1C0));
        }
        // Ensure unk10 default (0.50) if it was never set (typical when coming from a zeroed 0x20)
        if (oldSize <= FrameOff::kUnk10) {
            float f = 0.50f;
            ex.MemcpyScope(TfxExtern::Frame, FrameOff::kUnk10, &f, sizeof(f));
        }
    }
}

inline void FrameSetDefaults(ExternStorage& ex)
{
    // Set full struct defaults (matches your new FrameExtern POD)
    FrameExtern def{};
    // def.unk10 already 0.50f; def.unk1c0 already (1,1,0,1) in your struct
    ex.set(TfxExtern::Frame, def);
}

inline bool ShowFrameExternEditor(ExternStorage& ex)
{
    EnsureFrameCapacity(ex);

    auto rf = [&](size_t o) { return ex.getFloat(TfxExtern::Frame, o); };
    auto wf = [&](size_t o, float v) { ex.MemcpyScope(TfxExtern::Frame, o, &v, sizeof(v)); };

    bool changed = false;

    if (ImGui::BeginTable("frame_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Controls");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Reset Defaults")) { FrameSetDefaults(ex); changed = true; }

        auto rowF = [&](const char* label, size_t off, float step = 0.01f, float minv = -FLT_MAX, float maxv = FLT_MAX) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            float v = rf(off);
            if (ImGui::DragFloat(std::string("##").append(label).c_str(), &v, step, minv, maxv)) { wf(off, v); changed = true; }
            };

        rowF("game_time", FrameOff::kGameTime);
        rowF("render_time", FrameOff::kRenderTime);
        rowF("delta_game_time", FrameOff::kDeltaGameTime);
        rowF("exposure_time", FrameOff::kExposureTime);
        rowF("exposure_scale", FrameOff::kExposureScale, 0.01f, 0.0f, 16.0f);
        rowF("unk0c", FrameOff::kUnk0C);
        rowF("unk10", FrameOff::kUnk10);

        ImGui::EndTable();
    }
    return changed;
}

// ==================== GlobalLighting ====================
// ==================== GlobalLighting ====================
namespace GLightOff {
    // mirrors extern_struct! GlobalLightingExtern
    constexpr size_t kSRV08 = 0x08; // ID3D11ShaderResourceView*
    constexpr size_t kV10 = 0x10; // Vec4
    constexpr size_t kSpecularDir = 0x30; // Vec4 (unk30)
    constexpr size_t kDiffuseDir = 0x50; // Vec4 (unk50)
    constexpr size_t kV70 = 0x70; // Vec4
    constexpr size_t kV80 = 0x80; // Vec4
    constexpr size_t kF90 = 0x90; // float
    constexpr size_t kF94 = 0x94; // float (default -0.5)
    constexpr size_t kF98 = 0x98; // float
    constexpr size_t kF9C = 0x9C; // float
    constexpr size_t kFA0 = 0xA0; // float (unka0)
    constexpr size_t kVB0 = 0xB0; // Vec4
    constexpr size_t kVC0 = 0xC0; // Vec4
    constexpr size_t kVD0 = 0xD0; // Vec4
}

inline void EnsureGlobalLightingCapacity(ExternStorage& ex) {
    // If scope is missing/empty, seed full defaults from the POD struct.
    auto it = ex.scopes.find(TfxExtern::GlobalLighting);
    if (it == ex.scopes.end() || it->second.cpu.empty()) {
        GlobalLightingExtern def{};              // uses your C++ defaults
        ex.set(TfxExtern::GlobalLighting, def);  // copies sizeof(GlobalLightingExtern)
        return;
    }

    auto& scope = it->second;
    const size_t want = sizeof(GlobalLightingExtern); // 0xE0
    if (scope.cpu.size() < want) {
        const size_t oldSize = scope.cpu.size();
        scope.cpu.resize(want, 0u);
        scope.dirty = true;

        // Seed defaults only for newly-added tail regions (preserve existing front)
        auto seed_v4 = [&](size_t off, const Vec4& v) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::GlobalLighting, off, &v, sizeof(v));
            };
        auto seed_f = [&](size_t off, float f) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::GlobalLighting, off, &f, sizeof(f));
            };

        seed_v4(GLightOff::kV10, Vec4::one());
        seed_v4(GLightOff::kSpecularDir, Vec4(1.f, -1.f, 1.f, 0.f));
        seed_v4(GLightOff::kDiffuseDir, Vec4(1.f, -1.f, 1.f, 0.f));
        seed_v4(GLightOff::kV70, Vec4::one());
        seed_v4(GLightOff::kV80, Vec4::one());
        seed_f(GLightOff::kF90, 1.0f);
        seed_f(GLightOff::kF94, -0.5f);
        seed_f(GLightOff::kF98, 1.0f);
        seed_f(GLightOff::kF9C, 1.0f);
        seed_f(GLightOff::kFA0, 1.0f);
        seed_v4(GLightOff::kVB0, Vec4::one());
        seed_v4(GLightOff::kVC0, Vec4::one());
        seed_v4(GLightOff::kVD0, Vec4::one());
    }
}

inline void GlobalLightingSetDefaults(ExternStorage& ex) {
    GlobalLightingExtern def{};                 // all defaults from struct:
    // - SRV = nullptr
    // - Vec4s = Vec4::one() or (1,-1,1,0)
    // - floats = 1.0f / -0.5f as you set
    ex.set(TfxExtern::GlobalLighting, def);
}

inline bool ShowGlobalLightingExternEditor(ExternStorage& ex) {
    EnsureGlobalLightingCapacity(ex);

    auto rv = [&](size_t o) { return ex.getVec4(TfxExtern::GlobalLighting, o); };
    auto wv = [&](size_t o, const Vec4& v) { ex.MemcpyScope(TfxExtern::GlobalLighting, o, &v, sizeof(v)); };
    auto rf = [&](size_t o) { return ex.getFloat(TfxExtern::GlobalLighting, o); };
    auto wf = [&](size_t o, float f) { ex.MemcpyScope(TfxExtern::GlobalLighting, o, &f, sizeof(f)); };
    auto rs = [&](size_t o) { return ex.getSRV(TfxExtern::GlobalLighting, o); };
    auto ws_null = [&](size_t o) {
        ID3D11ShaderResourceView* p = nullptr;
        ex.MemcpyScope(TfxExtern::GlobalLighting, o, &p, sizeof(p));
        };

    bool changed = false;

    if (ImGui::BeginTable("gl_tbl", 2, ImGuiTableFlags_SizingStretchSame)) {
        // Header / controls
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Controls");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Reset Defaults")) { GlobalLightingSetDefaults(ex); changed = true; }

        // Small helpers
        auto rowV4 = [&](const char* label, size_t off, float step = 0.01f) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            Vec4 v = rv(off); float a[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::DragFloat4(std::string("##").append(label).c_str(), a, step)) {
                wv(off, Vec4(a[0], a[1], a[2], a[3])); changed = true;
            }
            };
        auto rowF = [&](const char* label, size_t off, float defStep = 0.01f) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            float f = rf(off);
            if (ImGui::DragFloat(std::string("##").append(label).c_str(), &f, defStep)) {
                wf(off, f); changed = true;
            }
            };
        auto rowSRV = [&](const char* label, size_t off) {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
            ImGui::TableSetColumnIndex(1);
            if (auto* p = rs(off)) {
                ImGui::Text("0x%p", (void*)p);
                ImGui::SameLine();
                if (ImGui::SmallButton(std::string("Clear##").append(label).c_str())) { ws_null(off); changed = true; }
            }
            else {
                ImGui::TextUnformatted("null");
                ImGui::SameLine();
                if (ImGui::SmallButton(std::string("Set null##").append(label).c_str())) { ws_null(off); changed = true; }
            }
            };

        // Fields
        rowSRV("unk08 (SRV)", GLightOff::kSRV08);

        rowV4("unk10", GLightOff::kV10);
        rowV4("specular_dir (unk30)", GLightOff::kSpecularDir);
        rowV4("diffuse_dir  (unk50)", GLightOff::kDiffuseDir);
        rowV4("unk70", GLightOff::kV70);
        rowV4("unk80", GLightOff::kV80);

        rowF("unk90", GLightOff::kF90);
        rowF("unk94", GLightOff::kF94);   // default -0.5
        rowF("unk98", GLightOff::kF98);
        rowF("unk9c", GLightOff::kF9C);
        rowF("unka0", GLightOff::kFA0);

        rowV4("unkb0", GLightOff::kVB0);
        rowV4("unkc0", GLightOff::kVC0);
        rowV4("unkd0", GLightOff::kVD0);

        ImGui::EndTable();
    }
    return changed;
}

