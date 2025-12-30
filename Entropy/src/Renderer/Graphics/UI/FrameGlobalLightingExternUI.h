// FrameGlobalLightingExternUI.h
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"


// ==================== Frame ====================
namespace FrameOff
{
    // --- scalars ---
    constexpr size_t kGameTime = 0x00;
    constexpr size_t kRenderTime = 0x04;
    constexpr size_t kUnk0C = 0x0C;
    constexpr size_t kUnk10 = 0x10; // default 0.50
    constexpr size_t kDeltaGameTime = 0x14;
    constexpr size_t kExposureTime = 0x18;
    constexpr size_t kExposureScale = 0x1C; // default 1.0

    constexpr size_t kUnk20 = 0x20;
    constexpr size_t kUnk24 = 0x24;
    constexpr size_t kExposureIllumRelative = 0x28;
    constexpr size_t kUnk2C = 0x2C;

    constexpr size_t kUnk40 = 0x40;
    constexpr size_t kUnk70 = 0x70;

    // --- SRV pointers (TextureView) ---
    constexpr size_t kUnk78 = 0x78;
    constexpr size_t kUnk80 = 0x80;
    constexpr size_t kUnk88 = 0x88;
    constexpr size_t kUnk90 = 0x90;
    constexpr size_t kUnk98 = 0x98;
    constexpr size_t kUnkA0 = 0xA0;

    constexpr size_t kSpecularLobeLookup = 0xA8;
    constexpr size_t kSpecularLobe3DLookup = 0xB0;
    constexpr size_t kSpecularTintLookup = 0xB8;
    constexpr size_t kIridescenceLookup = 0xC0;

    // --- Vec4 ---
    constexpr size_t kUnkD0 = 0xD0;
    constexpr size_t kUnk150 = 0x150;
    constexpr size_t kUnk160 = 0x160;
    constexpr size_t kUnk170 = 0x170;
    constexpr size_t kUnk180 = 0x180;

    // --- scalars ---
    constexpr size_t kUnk190 = 0x190;
    constexpr size_t kUnk194 = 0x194;

    // --- Vec4 defaults ---
    constexpr size_t kUnk1A0 = 0x1A0; // default zero
    constexpr size_t kUnk1B0 = 0x1B0; // default one
    constexpr size_t kUnk1C0 = 0x1C0; // default (1,1,0,1)

    // --- trailing SRVs ---
    constexpr size_t kUnk1E0 = 0x1E0;
    constexpr size_t kUnk1E8 = 0x1E8;
    constexpr size_t kUnk1F0 = 0x1F0;

    // whole struct size
    constexpr size_t kSize = sizeof(FrameExtern); // should be 0x1F8
}

inline void EnsureFrameCapacity(ExternStorage& ex)
{
    auto it = ex.scopes.find(TfxExtern::Frame);
    if (it == ex.scopes.end() || it->second.cpu.empty()) {
        ex.set(TfxExtern::Frame, FrameExtern{});
        return;
    }

    auto& scope = it->second;
    const size_t want = sizeof(FrameExtern);
    if (scope.cpu.size() < want) {
        const size_t oldSize = scope.cpu.size();
        scope.cpu.resize(want, 0u);
        scope.dirty = true;

        // Seed defaults only if that region was newly added
        auto seed_f = [&](size_t off, float v) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::Frame, off, &v, sizeof(v));
            };
        auto seed_v4 = [&](size_t off, const Vec4& v) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::Frame, off, &v, sizeof(v));
            };

        seed_f(FrameOff::kUnk10, 0.50f);
        seed_f(FrameOff::kExposureScale, 1.0f);

        seed_v4(FrameOff::kUnk1B0, Vec4::one());
        seed_v4(FrameOff::kUnk1C0, Vec4(1.0f, 1.0f, 0.0f, 1.0f));
    }
}

inline void FrameSetDefaults(ExternStorage& ex)
{
    FrameExtern def{};
    ex.set(TfxExtern::Frame, def);
}

inline bool ShowFrameExternEditor(ExternStorage& ex)
{
    EnsureFrameCapacity(ex);

    auto rf = [&](size_t o) { return ex.getFloat(TfxExtern::Frame, o); };
    auto wf = [&](size_t o, float v) { ex.MemcpyScope(TfxExtern::Frame, o, &v, sizeof(v)); };

    auto rv = [&](size_t o) { return ex.getVec4(TfxExtern::Frame, o); };
    auto wv = [&](size_t o, const Vec4& v) { ex.MemcpyScope(TfxExtern::Frame, o, &v, sizeof(v)); };

    auto rs = [&](size_t o) { return ex.getSRV(TfxExtern::Frame, o); };
    auto ws = [&](size_t o, ID3D11ShaderResourceView* p) { ex.MemcpyScope(TfxExtern::Frame, o, &p, sizeof(p)); };

    bool changed = false;

    if (ImGui::Button("Reset Frame Defaults")) { FrameSetDefaults(ex); changed = true; }
    ImGui::Separator();

    // ---- Scalars ----
    if (ImGui::CollapsingHeader("Frame Scalars", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto rowF = [&](const char* label, size_t off, float step = 0.01f, float minv = -FLT_MAX, float maxv = FLT_MAX) {
            float v = rf(off);
            if (ImGui::DragFloat(label, &v, step, minv, maxv)) { wf(off, v); changed = true; }
            };

        rowF("game_time", FrameOff::kGameTime);
        rowF("render_time", FrameOff::kRenderTime);
        rowF("delta_game_time", FrameOff::kDeltaGameTime);
        rowF("exposure_time", FrameOff::kExposureTime);
        rowF("exposure_scale", FrameOff::kExposureScale, 0.01f, 0.0f, 16.0f);

        rowF("unk0c", FrameOff::kUnk0C);
        rowF("unk10 (default 0.50)", FrameOff::kUnk10, 0.01f, 0.0f, 4.0f);
        rowF("unk20", FrameOff::kUnk20);
        rowF("unk24", FrameOff::kUnk24);
        rowF("exposure_illum_relative", FrameOff::kExposureIllumRelative);
        rowF("unk2c", FrameOff::kUnk2C);
        rowF("unk40", FrameOff::kUnk40);
        rowF("unk70", FrameOff::kUnk70);
        rowF("unk190", FrameOff::kUnk190);
        rowF("unk194", FrameOff::kUnk194);
    }

    // ---- SRVs ----
    if (ImGui::CollapsingHeader("Frame TextureViews (SRVs)", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto rowSRV = [&](const char* label, size_t off) {
            ID3D11ShaderResourceView* p = rs(off);
            ImGui::Text("%s = %p", label, (void*)p);
            ImGui::SameLine();
            if (ImGui::SmallButton((std::string("Clear##") + label).c_str())) {
                ID3D11ShaderResourceView* nullp = nullptr;
                ws(off, nullp);
                changed = true;
            }
            };

        rowSRV("unk78", FrameOff::kUnk78);
        rowSRV("unk80", FrameOff::kUnk80);
        rowSRV("unk88", FrameOff::kUnk88);
        rowSRV("unk90", FrameOff::kUnk90);
        rowSRV("unk98", FrameOff::kUnk98);
        rowSRV("unka0", FrameOff::kUnkA0);

        rowSRV("specular_lobe_lookup (0xA8)", FrameOff::kSpecularLobeLookup);
        rowSRV("specular_lobe_3d_lookup (0xB0)", FrameOff::kSpecularLobe3DLookup);
        rowSRV("specular_tint_lookup (0xB8)", FrameOff::kSpecularTintLookup);
        rowSRV("iridescence_lookup (0xC0)", FrameOff::kIridescenceLookup);

        rowSRV("unk1e0", FrameOff::kUnk1E0);
        rowSRV("unk1e8", FrameOff::kUnk1E8);
        rowSRV("unk1f0", FrameOff::kUnk1F0);
    }

    // ---- Vec4s ----
    if (ImGui::CollapsingHeader("Frame Vec4", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto rowV4 = [&](const char* label, size_t off, float step = 0.01f) {
            Vec4 v = rv(off);
            float a[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::DragFloat4(label, a, step)) {
                wv(off, Vec4(a[0], a[1], a[2], a[3]));
                changed = true;
            }
            };

        rowV4("unkd0", FrameOff::kUnkD0);
        rowV4("unk150", FrameOff::kUnk150);
        rowV4("unk160", FrameOff::kUnk160);
        rowV4("unk170", FrameOff::kUnk170);
        rowV4("unk180", FrameOff::kUnk180);

        rowV4("unk1a0 (default zero)", FrameOff::kUnk1A0);
        rowV4("unk1b0 (default one)", FrameOff::kUnk1B0);
        rowV4("unk1c0 (default 1,1,0,1)", FrameOff::kUnk1C0);
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

