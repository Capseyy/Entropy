
#pragma once
#include "TigerEngine/Technique/Tfx/extern.h"
#include "Renderer/Graphics/ImGui/imgui.h"


namespace AtmosphereOff {
    constexpr size_t kTimeOfDayN = 0x70;   
    constexpr size_t kLookupRes = 0x90;   
    constexpr size_t kDepthAngleRes = 0xD0;   
    constexpr size_t kFarLookupSRV = 0xE0;   
    constexpr size_t kNearLookupSRV = 0xF0;   
    constexpr size_t kUnk110 = 0x110;  
    constexpr size_t kFogColor = 0x140;  
    constexpr size_t kFogIntensity = 0x160;  
    constexpr size_t kUnk170 = 0x170;  
    constexpr size_t kUnk198 = 0x198;  
    constexpr size_t kRot1B4 = 0x1B4;  
    constexpr size_t kIntensity1B8 = 0x1B8;  
    constexpr size_t kCutoff1BC = 0x1BC;  
    constexpr size_t kUnk210 = 0x210;  
    
}

inline void EnsureAtmosphereCapacity(ExternStorage& ex)
{
    
    auto& scope = ex.scopes[TfxExtern::Atmosphere];
    const size_t need = 0x214 + sizeof(Vec4);
    if (scope.cpu.size() < need) {
        scope.cpu.resize(need, 0u);
        scope.dirty = true;
    }
}

inline void AtmosphereSetDefaults(ExternStorage& ex)
{
    EnsureAtmosphereCapacity(ex);

    
    { float v = 0.5f; ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kTimeOfDayN, &v, sizeof(v)); }

    
    
    {
        Vec4 v(512.f, 512.f, 1.f / 512.f, 1.f / 512.f);
        ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kDepthAngleRes, &v, sizeof(v));
    }

    
    {
        Vec4 v(0.f, 0.f, -1.5f, 0.f);
        ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kUnk110, &v, sizeof(v));
    }

    
    { Vec4 c = Vec4::zero(); ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kFogColor, &c, sizeof(c)); }
    { float f = 0.0f; ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kFogIntensity, &f, sizeof(f)); }

    
    { float f = 0.0001f; ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kUnk170, &f, sizeof(f)); }
    { float f = 0.0001f; ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kUnk198, &f, sizeof(f)); }
    { float f = 0.0f;    ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kRot1B4, &f, sizeof(f)); }
    { float f = 0.5f;    ex.MemcpyScope(TfxExtern::Atmosphere, AtmosphereOff::kCutoff1BC, &f, sizeof(f)); }
}

inline bool ShowAtmosphereExternEditor(ExternStorage& ex)
{
    EnsureAtmosphereCapacity(ex);
    bool changed = false;

    auto rdF = [&](size_t off) { return ex.getFloat(TfxExtern::Atmosphere, off); };
    auto wrF = [&](size_t off, float v) { ex.MemcpyScope(TfxExtern::Atmosphere, off, &v, sizeof(v)); changed = true; };
    auto rdV4 = [&](size_t off) { return ex.getVec4(TfxExtern::Atmosphere, off); };
    auto wrV4 = [&](size_t off, const Vec4& v) { ex.MemcpyScope(TfxExtern::Atmosphere, off, &v, sizeof(v)); changed = true; };
    auto rdSRV = [&](size_t off) { return ex.getSRV(TfxExtern::Atmosphere, off); };
    auto wrSRV = [&](size_t off, ID3D11ShaderResourceView* p)
        { ex.MemcpyScope(TfxExtern::Atmosphere, off, &p, sizeof(p)); changed = true; };

    if (ImGui::BeginTable("atm_tbl", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0);
        ImGui::TextUnformatted("Controls");
        ImGui::TableSetColumnIndex(1);
        if (ImGui::Button("Reset Defaults")) { AtmosphereSetDefaults(ex); changed = true; }

        
        {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Time of day (0..1)");
            ImGui::TableSetColumnIndex(1);
            float f = rdF(AtmosphereOff::kTimeOfDayN);
            if (ImGui::SliderFloat("##tod", &f, 0.f, 1.f)) wrF(AtmosphereOff::kTimeOfDayN, f);
        }

        
        {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Atmos lookup res (xy, 1/x, 1/y)");
            ImGui::TableSetColumnIndex(1);
            Vec4 v = rdV4(AtmosphereOff::kLookupRes);
            float a[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::DragFloat4("##atm_res", a, 1.f)) { v = Vec4(a[0], a[1], a[2], a[3]); wrV4(AtmosphereOff::kLookupRes, v); }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Depth-angle density res");
            ImGui::TableSetColumnIndex(1);
            v = rdV4(AtmosphereOff::kDepthAngleRes);
            a[0] = v.x; a[1] = v.y; a[2] = v.z; a[3] = v.w;
            if (ImGui::DragFloat4("##dad_res", a, 1.f)) { v = Vec4(a[0], a[1], a[2], a[3]); wrV4(AtmosphereOff::kDepthAngleRes, v); }
        }

        
        {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Fog color");
            ImGui::TableSetColumnIndex(1);
            Vec4 v = rdV4(AtmosphereOff::kFogColor);
            float c[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::ColorEdit4("##fog_color", c, ImGuiColorEditFlags_Float)) {
                v = Vec4(c[0], c[1], c[2], c[3]); wrV4(AtmosphereOff::kFogColor, v);
            }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Fog intensity");
            ImGui::TableSetColumnIndex(1);
            float fi = rdF(AtmosphereOff::kFogIntensity);
            if (ImGui::DragFloat("##fog_intensity", &fi, 0.01f, 0.f, 10.f)) wrF(AtmosphereOff::kFogIntensity, fi);
        }

        
        {
            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Unk110 (default Z * -1.5)");
            ImGui::TableSetColumnIndex(1);
            Vec4 v = rdV4(AtmosphereOff::kUnk110);
            float a[4]{ v.x, v.y, v.z, v.w };
            if (ImGui::DragFloat4("##unk110", a, 0.01f)) { v = Vec4(a[0], a[1], a[2], a[3]); wrV4(AtmosphereOff::kUnk110, v); }

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Rotation 1B4");
            ImGui::TableSetColumnIndex(1);
            float rot = rdF(AtmosphereOff::kRot1B4);
            if (ImGui::DragFloat("##rot1b4", &rot, 0.1f)) wrF(AtmosphereOff::kRot1B4, rot);

            ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::Text("Cutoff 1BC");
            ImGui::TableSetColumnIndex(1);
            float cut = rdF(AtmosphereOff::kCutoff1BC);
            if (ImGui::DragFloat("##cut1bc", &cut, 0.01f, 0.f, 1.f)) wrF(AtmosphereOff::kCutoff1BC, cut);
        }

        
        {
            auto showSrv = [&](const char* label, size_t off) {
                ImGui::TableNextRow(); ImGui::TableSetColumnIndex(0); ImGui::TextUnformatted(label);
                ImGui::TableSetColumnIndex(1);
                ID3D11ShaderResourceView* srv = rdSRV(off);
                ImGui::Text("0x%p", (void*)srv);
                ImGui::SameLine();
                if (ImGui::SmallButton(std::string("Clear##").append(label).c_str())) { wrSRV(off, nullptr); }
                };
            showSrv("atmos_ss_far_lookup (SRV*)", AtmosphereOff::kFarLookupSRV);
            showSrv("atmos_ss_near_lookup (SRV*)", AtmosphereOff::kNearLookupSRV);
        }

        ImGui::EndTable();
    }

    return changed;
}
