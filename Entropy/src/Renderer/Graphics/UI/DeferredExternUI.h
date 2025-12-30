// DeferredExternUI.h
#pragma once

#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"

// ==================== Deferred ====================
// Mirrors extern_struct! DeferredExtern (see TigerEngine/Technique/Tfx/extern.h)
namespace DeferredOff
{
    // ---- constants ----
    constexpr size_t kDepthConstants = 0x00; // Vec4
    constexpr size_t kUnk10          = 0x10; // Vec4
    constexpr size_t kUnk20          = 0x20; // Vec4
    constexpr size_t kUnk30          = 0x30; // float

    // ---- SRV pointers (TextureView) ----
    constexpr size_t kDeferredDepth       = 0x38;
    constexpr size_t kGBufferAlbedo       = 0x48;
    constexpr size_t kGBufferNormal       = 0x50;
    constexpr size_t kGBufferMaterial     = 0x58;

    constexpr size_t kLightDiffuse        = 0x60;
    constexpr size_t kLightSpecular       = 0x68;
    constexpr size_t kLightIblSpecular    = 0x70;

    constexpr size_t kSsao               = 0x78;
    constexpr size_t kShadowMask         = 0x80;
    constexpr size_t kUnk88              = 0x88;
    constexpr size_t kUnk90              = 0x90;
    constexpr size_t kSkyHemisphereMips  = 0x98;

    constexpr size_t kSize = sizeof(DeferredExtern); // should be 0xA0
}

inline void EnsureDeferredCapacity(ExternStorage& ex)
{
    auto it = ex.scopes.find(TfxExtern::Deferred);
    if (it == ex.scopes.end() || it->second.cpu.empty()) {
        ex.set(TfxExtern::Deferred, DeferredExtern{});
        return;
    }

    auto& scope = it->second;
    const size_t want = sizeof(DeferredExtern);
    if (scope.cpu.size() < want) {
        const size_t oldSize = scope.cpu.size();
        scope.cpu.resize(want, 0u);
        scope.dirty = true;

        // Seed defaults only if that region was newly added.
        auto seed_v4 = [&](size_t off, const Vec4& v) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::Deferred, off, &v, sizeof(v));
        };
        auto seed_f = [&](size_t off, float v) {
            if (oldSize <= off) ex.MemcpyScope(TfxExtern::Deferred, off, &v, sizeof(v));
        };

        seed_v4(DeferredOff::kDepthConstants, Vec4(0.0f, 1.0f / 0.01f, 0.0f, 0.0f));
        seed_v4(DeferredOff::kUnk10, Vec4::zero());
        seed_v4(DeferredOff::kUnk20, Vec4::zero());
        seed_f(DeferredOff::kUnk30, 0.0f);
    }
}

inline void DeferredSetDefaults(ExternStorage& ex)
{
    DeferredExtern def{};
    ex.set(TfxExtern::Deferred, def);
}

inline bool ShowDeferredExternEditor(ExternStorage& ex)
{
    EnsureDeferredCapacity(ex);

    auto rf = [&](size_t o) { return ex.getFloat(TfxExtern::Deferred, o); };
    auto wf = [&](size_t o, float v) { ex.MemcpyScope(TfxExtern::Deferred, o, &v, sizeof(v)); };

    auto rv = [&](size_t o) { return ex.getVec4(TfxExtern::Deferred, o); };
    auto wv = [&](size_t o, const Vec4& v) { ex.MemcpyScope(TfxExtern::Deferred, o, &v, sizeof(v)); };

    auto rs = [&](size_t o) { return ex.getSRV(TfxExtern::Deferred, o); };
    auto ws = [&](size_t o, ID3D11ShaderResourceView* p) { ex.MemcpyScope(TfxExtern::Deferred, o, &p, sizeof(p)); };

    bool changed = false;

    if (ImGui::Button("Reset Deferred Defaults")) { DeferredSetDefaults(ex); changed = true; }
    ImGui::Separator();

    // ---- Constants ----
    if (ImGui::CollapsingHeader("Deferred Constants", ImGuiTreeNodeFlags_DefaultOpen))
    {
        auto rowV4 = [&](const char* label, size_t off, float step = 0.01f) {
            Vec4 v = rv(off);
            float a[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::DragFloat4(label, a, step)) {
                wv(off, Vec4(a[0], a[1], a[2], a[3]));
                changed = true;
            }
        };
        auto rowF = [&](const char* label, size_t off, float step = 0.01f, float minv = -FLT_MAX, float maxv = FLT_MAX) {
            float v = rf(off);
            if (ImGui::DragFloat(label, &v, step, minv, maxv)) { wf(off, v); changed = true; }
        };

        rowV4("depth_constants", DeferredOff::kDepthConstants);
        rowV4("unk10", DeferredOff::kUnk10);
        rowV4("unk20", DeferredOff::kUnk20);
        rowF("unk30", DeferredOff::kUnk30);
    }

    // ---- SRVs ----
    if (ImGui::CollapsingHeader("Deferred TextureViews (SRVs)", ImGuiTreeNodeFlags_DefaultOpen))
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

        rowSRV("deferred_depth (t0)", DeferredOff::kDeferredDepth);

        ImGui::SeparatorText("GBuffer");
        rowSRV("gbuffer_albedo (t1)", DeferredOff::kGBufferAlbedo);
        rowSRV("gbuffer_normal (t2)", DeferredOff::kGBufferNormal);
        rowSRV("gbuffer_material (t3)", DeferredOff::kGBufferMaterial);

        ImGui::SeparatorText("Lighting");
        rowSRV("light_diffuse (t4)", DeferredOff::kLightDiffuse);
        rowSRV("light_specular (t5)", DeferredOff::kLightSpecular);
        rowSRV("light_ibl_specular (t6)", DeferredOff::kLightIblSpecular);

        ImGui::SeparatorText("Post / Masks");
        rowSRV("ssao (t7)", DeferredOff::kSsao);
        rowSRV("shadow_mask (t8)", DeferredOff::kShadowMask);
        rowSRV("unk88", DeferredOff::kUnk88);
        rowSRV("unk90", DeferredOff::kUnk90);
        rowSRV("sky_hemisphere_mips", DeferredOff::kSkyHemisphereMips);
    }

    return changed;
}
