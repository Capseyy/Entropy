#include "Runtime/Assets/Technique.h"
#undef min
#undef max
#include "TigerEngine/Technique/Tfx/tfx_program.h"
#undef TRANSPARENT


namespace TfxScope{
    using Bits = uint64_t;
    enum : Bits {
        FRAME = 1ull << 0, VIEW = 1ull << 1, RIGID_MODEL = 1ull << 2, EDITOR_MESH = 1ull << 3,
        EDITOR_TERRAIN = 1ull << 4, CUI_VIEW = 1ull << 5, CUI_OBJECT = 1ull << 6, SKINNING = 1ull << 7,
        SPEEDTREE = 1ull << 8, CHUNK_MODEL = 1ull << 9, DECAL = 1ull << 10, INSTANCES = 1ull << 11,
        SPEEDTREE_LOD_DRAWCALL_DATA = 1ull << 12, TRANSPARENT = 1ull << 13, TRANSPARENT_ADVANCED = 1ull << 14,
        SDSM_BIAS_AND_SCALE_TEXTURES = 1ull << 15, TERRAIN = 1ull << 16, POSTPROCESS = 1ull << 17,
        CUI_BITMAP = 1ull << 18, CUI_STANDARD = 1ull << 19, UI_FONT = 1ull << 20, CUI_HUD = 1ull << 21,
        PARTICLE_TRANSFORMS = 1ull << 22, PARTICLE_LOCATION_METADATA = 1ull << 23, CUBEMAP_VOLUME = 1ull << 24,
        GEAR_PLATED_TEXTURES = 1ull << 25, GEAR_DYE_0 = 1ull << 26, GEAR_DYE_1 = 1ull << 27, GEAR_DYE_2 = 1ull << 28,
        GEAR_DYE_DECAL = 1ull << 29, GENERIC_ARRAY = 1ull << 30, GEAR_DYE_SKIN = 1ull << 31, GEAR_DYE_LIPS = 1ull << 32,
        GEAR_DYE_HAIR = 1ull << 33, GEAR_DYE_FACIAL_LAYER_0_MASK = 1ull << 34, GEAR_DYE_FACIAL_LAYER_0_MATERIAL = 1ull << 35,
        GEAR_DYE_FACIAL_LAYER_1_MASK = 1ull << 36, GEAR_DYE_FACIAL_LAYER_1_MATERIAL = 1ull << 37,
        PLAYER_CENTERED_CASCADED_GRID = 1ull << 38, GEAR_DYE_012 = 1ull << 39, COLOR_GRADING_UBERSHADER = 1ull << 40
    };
    inline Bits from_bits_truncate(Bits v) { return v & ((Bits(1) << 41) - 1); }
    inline bool has(Bits s, Bits b) { return (s & b) != 0; }
}


struct DecodedSelection {
    std::optional<uint8_t> blend;
    std::optional<uint8_t> depthStencilCombo; // index into DEPTH_STENCIL_COMBOS
    std::optional<uint8_t> rasterizer;        // 0..8
    std::optional<uint8_t> depthBias;         // 0..8
};

static inline DecodedSelection DecodeStateSelection(uint32_t sel) {
    auto get = [&](int shift)->std::optional<uint8_t> {
        uint8_t v = uint8_t((sel >> shift) & 0xFF);
        return (v & 0x80) ? std::optional<uint8_t>(v & 0x7F) : std::nullopt;
        };
    return {
        /*blend*/            get(0),
        /*depthStencilCombo*/get(8),
        /*rasterizer*/       get(16),
        /*depthBias*/        get(24)
    };
}

bool EntropyAssets::Technique::Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes)
{
    uint32_t StateSelection = this->StateSelection;

    uint64_t UsedScopes = this->usedScopes;
    auto used = TfxScope::from_bits_truncate(this->usedScopes);

    const auto sel = DecodeStateSelection(this->StateSelection);

    // Blend ----------------------------------------------------------
    if (sel.blend && *sel.blend < states.blend_states.size() &&
        states.blend_states[*sel.blend]) {
        const float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };
        const UINT sampleMask = 0xFFFFFFFFu;
        //printf("Mapped blend\n");
        pContext->OMSetBlendState(states.blend_states[*sel.blend].Get(), blendFactor, sampleMask);
    }

    if (sel.rasterizer && *sel.rasterizer < 9) {
        auto& rs = states.rasterizer_states[*sel.rasterizer];
        if (rs) {
            pContext->RSSetState(rs.Get());
        }
    }

    if (!this->VS.empty()) {

        ID3D11VertexShader* vs = this->VS[0]->vs.Get();
        pContext->VSSetShader(vs, nullptr, 0);
    }
    if (!this->PS.empty()) {

        ID3D11PixelShader* ps = this->PS[0]->ps.Get();
        pContext->PSSetShader(ps, nullptr, 0);
    }
    TfxProgram prog = TfxProgram::FromBytecode(this->pixeldata.TFX_Bytecode,
        this->pixeldata.TFX_Constants, this->id);

    auto& cb0 = this->pixeldata.SamplerFallback;
    
    prog.Evaluate(externs, cb0, this->Textures);
 
    if (this->CBuffers.empty() && this->CBuffers_fallback != nullptr)  {
        ID3D11Buffer* buf = this->CBuffers_fallback->buffer.Get();

        D3D11_BUFFER_DESC desc{};
        buf->GetDesc(&desc);

        const size_t bytes_needed = cb0.size() * sizeof(Vec4);
        const size_t bytes_copy = std::min<size_t>(bytes_needed, desc.ByteWidth);

        if (desc.Usage == D3D11_USAGE_DYNAMIC) {
            D3D11_MAPPED_SUBRESOURCE map{};
            if (SUCCEEDED(pContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
                std::memcpy(map.pData, cb0.data(), bytes_copy);
                pContext->Unmap(buf, 0);
            }
        }
        else {
            pContext->UpdateSubresource(buf, 0, nullptr, cb0.data(), 0, 0);
        }
        pContext->PSSetConstantBuffers(UINT(this->psCBSlots_fallback), 1, &buf);
    }

    if (!this->CBuffers.empty()) {
        for (size_t i = 0; i < this->CBuffers.size(); ++i) {
            ID3D11Buffer* b = this->CBuffers[i]->buffer.Get();
            pContext->PSSetConstantBuffers(UINT(i), 1, &b);
        }
    }

    struct PsSamplerBind { UINT slot; UINT sampler_index; };
    std::vector<PsSamplerBind> psBinds;

    {
        // Minimal stack for sampler indices
        std::vector<int> sstack; sstack.reserve(16);

        auto ops = ParseAll(this->pixeldata.TFX_Bytecode, /*trace=*/false);
        for (const auto& op : ops) {
            switch (op.op) {
            case TfxBytecode::PushSampler: {
                auto d = std::get<PushSamplerData>(op.data);
                sstack.push_back(int(d.index));
                break;
            }
            case TfxBytecode::SetShaderSampler: {
                auto d = std::get<SetShaderBindingData>(op.data);
                // stage: 1 == Pixel (from your EoF table)
                if (!sstack.empty()) {
                    int idx = sstack.back(); sstack.pop_back();
                    if (d.stage == 1) {
                        psBinds.push_back(PsSamplerBind{ UINT(d.slot), UINT(idx) });
                    }
                }
                break;
            }
            default: break;
            }
        }
    }
    for (const auto& b : psBinds) {
        if (b.sampler_index < this->Samplers.size() && this->Samplers[b.sampler_index]) {
            ID3D11SamplerState* s = this->Samplers[b.sampler_index]->sampler.Get();
            pContext->PSSetSamplers(b.slot, 1, &s);
        }
    }


    if (psBinds.empty()) {
        for (size_t i = 0; i < this->Samplers.size(); ++i) {
            ID3D11SamplerState* s = this->Samplers[i]->sampler.Get();
            pContext->PSSetSamplers(UINT(i), 1, &s); 
        }
    }

    if (!this->Textures.empty()) {
        const size_t n = std::min(this->Textures.size(), this->psTextureSlots.size());
        for (size_t i = 0; i < n; ++i) {
            const UINT slot = this->psTextureSlots[i];
            ID3D11ShaderResourceView* srv = this->Textures[i] ? this->Textures[i]->Get() : nullptr;
            pContext->PSSetShaderResources(slot, 1, &srv);
        }
    }
    if (!this->Textures3D.empty()) {
        const size_t n = std::min(this->Textures3D.size(), this->psTextureSlots3D.size());
        for (size_t i = 0; i < n; ++i) {
            const UINT slot = this->psTextureSlots3D[i];
            ID3D11ShaderResourceView* srv = this->Textures3D[i] ? this->Textures3D[i]->Get() : nullptr;
            pContext->PSSetShaderResources(slot, 1, &srv);
        }
    }

    if (this->vertexdata.TFX_Bytecode.size() != 0 && this->vertexdata.contstant_buffer.hash == 0xffffffff) {
        TfxProgram prog_vs = TfxProgram::FromBytecode(this->vertexdata.TFX_Bytecode,
            this->vertexdata.TFX_Constants, this->id);
        auto& cb0_vs = this->vertexdata.SamplerFallback;
        prog_vs.Evaluate(externs, cb0_vs,this->Textures_VS);
        if (this->CBuffers_VS.empty() && this->CBuffers_fallback_VS != nullptr) {
            ID3D11Buffer* buf = this->CBuffers_fallback_VS->buffer.Get();

            D3D11_BUFFER_DESC desc{};
            buf->GetDesc(&desc);

            const size_t bytes_needed = cb0_vs.size() * sizeof(Vec4);
            const size_t bytes_copy = std::min<size_t>(bytes_needed, desc.ByteWidth);

            if (desc.Usage == D3D11_USAGE_DYNAMIC) {
                D3D11_MAPPED_SUBRESOURCE map{};
                if (SUCCEEDED(pContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
                    std::memcpy(map.pData, cb0_vs.data(), bytes_copy);
                    pContext->Unmap(buf, 0);
                }
            }
            else {
                pContext->UpdateSubresource(buf, 0, nullptr, cb0_vs.data(), 0, 0);
            }
            pContext->VSSetConstantBuffers(UINT(this->vsCBSlots_fallback), 1, &buf);
        }
    }
    // Bind all technique constant buffers (your existing behavior)
    if (!this->CBuffers_VS.empty()) {
        for (size_t i = 0; i < this->CBuffers_VS.size(); ++i) {
            ID3D11Buffer* b = this->CBuffers_VS[i]->buffer.Get();
            pContext->VSSetConstantBuffers(UINT(i), 1, &b);
        }
    }


    // Bind scopes explicitly by bit, matching enum order (index == bit position)
    if (TfxScope::has(used, TfxScope::FRAME)) { scopes[0].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::VIEW)) { scopes[1].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::RIGID_MODEL)) { scopes[2].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::EDITOR_MESH)) { scopes[3].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::EDITOR_TERRAIN)) { scopes[4].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_VIEW)) { scopes[5].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_OBJECT)) { scopes[6].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SKINNING)) { scopes[7].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SPEEDTREE)) { scopes[8].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CHUNK_MODEL)) { scopes[9].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::DECAL)) { scopes[10].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::INSTANCES)) { scopes[11].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SPEEDTREE_LOD_DRAWCALL_DATA)) { scopes[12].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TRANSPARENT)) { scopes[13].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TRANSPARENT_ADVANCED)) { scopes[14].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SDSM_BIAS_AND_SCALE_TEXTURES)) { scopes[15].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TERRAIN)) { scopes[16].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::POSTPROCESS)) { scopes[17].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_BITMAP)) { scopes[18].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_STANDARD)) { scopes[19].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::UI_FONT)) { scopes[20].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_HUD)) { scopes[21].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PARTICLE_TRANSFORMS)) { scopes[22].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PARTICLE_LOCATION_METADATA)) { scopes[23].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUBEMAP_VOLUME)) { scopes[24].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_PLATED_TEXTURES)) { scopes[25].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_0)) { scopes[26].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_1)) { scopes[27].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_2)) { scopes[28].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_DECAL)) { scopes[29].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GENERIC_ARRAY)) { scopes[30].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_SKIN)) { scopes[31].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_LIPS)) { scopes[32].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_HAIR)) { scopes[33].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_0_MASK)) { scopes[34].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_0_MATERIAL)) { scopes[35].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_1_MASK)) { scopes[36].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_1_MATERIAL)) { scopes[37].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PLAYER_CENTERED_CASCADED_GRID)) { scopes[38].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_012)) { scopes[39].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::COLOR_GRADING_UBERSHADER)) { scopes[40].second.Bind(pContext); }


    return true;
}

bool EntropyAssets::Technique::Bind_With_Channels(Microsoft::WRL::ComPtr<ID3D11Device> pDevice,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes, std::unordered_map<uint32_t, float_t> channels)
{
    uint32_t StateSelection = this->StateSelection;

    uint64_t UsedScopes = this->usedScopes;
    auto used = TfxScope::from_bits_truncate(this->usedScopes);

    const auto sel = DecodeStateSelection(this->StateSelection);

    // Blend ----------------------------------------------------------
    if (sel.blend && *sel.blend < states.blend_states.size() &&
        states.blend_states[*sel.blend]) {
        const float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };
        const UINT sampleMask = 0xFFFFFFFFu;
        //printf("Mapped blend\n");
        pContext->OMSetBlendState(states.blend_states[*sel.blend].Get(), blendFactor, sampleMask);
    }

    if (sel.rasterizer && *sel.rasterizer < 9) {
        auto& rs = states.rasterizer_states[*sel.rasterizer];
        if (rs) {
            pContext->RSSetState(rs.Get());
        }
    }

    if (!this->VS.empty()) {

        ID3D11VertexShader* vs = this->VS[0]->vs.Get();
        pContext->VSSetShader(vs, nullptr, 0);
    }
    if (!this->PS.empty()) {

        ID3D11PixelShader* ps = this->PS[0]->ps.Get();
        pContext->PSSetShader(ps, nullptr, 0);
    }
    
    TfxProgram prog = TfxProgram::FromBytecode(this->pixeldata.TFX_Bytecode,
        this->pixeldata.TFX_Constants, this->id);
    // implement getFloat/getVec4/getMat4 in extern.cpp
    auto& cb0 = this->pixeldata.SamplerFallback; // std::vector<Vec4> used as cb0 backing
    /*if (this->id == 0x80CE12FC)
    {
		printf("Start EVAL\n");
		printf("Technique::Bind_With_Channels: Using Evaluate_With_Channels with technique 0x810AF322\n");
        prog.Evaluate_With_Channels(externs, cb0, channels, this->Textures, true);
		printf("End EVAL\n");
    }*/
    {
        
        prog.Evaluate_With_Channels(externs, cb0, channels, this->Textures, false);

    }

    


    // writes float4s to cb0[..] as dictated by bytecode

    if (this->CBuffers.empty() && this->CBuffers_fallback != nullptr) {
        ID3D11Buffer* buf = this->CBuffers_fallback->buffer.Get();

        D3D11_BUFFER_DESC desc{};
        buf->GetDesc(&desc);

        const size_t bytes_needed = cb0.size() * sizeof(Vec4);
        const size_t bytes_copy = std::min<size_t>(bytes_needed, desc.ByteWidth);

        if (desc.Usage == D3D11_USAGE_DYNAMIC) {
            D3D11_MAPPED_SUBRESOURCE map{};
            if (SUCCEEDED(pContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
                std::memcpy(map.pData, cb0.data(), bytes_copy);
                pContext->Unmap(buf, 0);
            }
        }
        else {
            pContext->UpdateSubresource(buf, 0, nullptr, cb0.data(), 0, 0);
        }
        pContext->PSSetConstantBuffers(UINT(this->psCBSlots_fallback), 1, &buf);
    }

    // Bind all technique constant buffers (your existing behavior)
    if (!this->CBuffers.empty()) {
        for (size_t i = 0; i < this->CBuffers.size(); ++i) {
            ID3D11Buffer* b = this->CBuffers[i]->buffer.Get();
            pContext->PSSetConstantBuffers(UINT(i), 1, &b);
        }
    }

    // ---- 3) Parse bytecode to bind the *correct* sampler slots ----
    // We read PushSampler + SetShaderSampler (stage,slot) and build a mapping.
    struct PsSamplerBind { UINT slot; UINT sampler_index; };
    std::vector<PsSamplerBind> psBinds;

    {
        // Minimal stack for sampler indices
        std::vector<int> sstack; sstack.reserve(16);

        auto ops = ParseAll(this->pixeldata.TFX_Bytecode, /*trace=*/false);
        for (const auto& op : ops) {
            switch (op.op) {
            case TfxBytecode::PushSampler: {
                auto d = std::get<PushSamplerData>(op.data);
                sstack.push_back(int(d.index));
                break;
            }
            case TfxBytecode::SetShaderSampler: {
                auto d = std::get<SetShaderBindingData>(op.data);
                // stage: 1 == Pixel (from your EoF table)
                if (!sstack.empty()) {
                    int idx = sstack.back(); sstack.pop_back();
                    if (d.stage == 1) {
                        psBinds.push_back(PsSamplerBind{ UINT(d.slot), UINT(idx) });
                    }
                }
                break;
            }
            default: break;
            }
        }
    }

    // Bind samplers exactly to the slots requested by the bytecode
    for (const auto& b : psBinds) {
        if (b.sampler_index < this->Samplers.size() && this->Samplers[b.sampler_index]) {
            ID3D11SamplerState* s = this->Samplers[b.sampler_index]->sampler.Get();
            pContext->PSSetSamplers(b.slot, 1, &s);
        }
    }

    // (Optional) If there were no SetShaderSampler ops, fall back to your old linear binding:
    if (psBinds.empty()) {
        for (size_t i = 0; i < this->Samplers.size(); ++i) {
            ID3D11SamplerState* s = this->Samplers[i]->sampler.Get();
            pContext->PSSetSamplers(UINT(i), 1, &s); // no +1 offset anymore
        }
    }

    if (!this->Textures.empty()) {
        const size_t n = std::min(this->Textures.size(), this->psTextureSlots.size());
        for (size_t i = 0; i < n; ++i) {
            const UINT slot = this->psTextureSlots[i];
            ID3D11ShaderResourceView* srv = this->Textures[i] ? this->Textures[i]->Get() : nullptr;
            pContext->PSSetShaderResources(slot, 1, &srv);
        }
    }
    if (!this->Textures3D.empty()) {
        const size_t n = std::min(this->Textures3D.size(), this->psTextureSlots3D.size());
        for (size_t i = 0; i < n; ++i) {
            const UINT slot = this->psTextureSlots3D[i];
            ID3D11ShaderResourceView* srv = this->Textures3D[i] ? this->Textures3D[i]->Get() : nullptr;
            pContext->PSSetShaderResources(slot, 1, &srv);
        }
    }

    if (this->vertexdata.TFX_Bytecode.size() != 0 && this->vertexdata.contstant_buffer.hash == 0xffffffff) {
        TfxProgram prog_vs = TfxProgram::FromBytecode(this->vertexdata.TFX_Bytecode,
            this->vertexdata.TFX_Constants, this->id);
        auto& cb0_vs = this->vertexdata.SamplerFallback;
        prog_vs.Evaluate_With_Channels(externs, cb0_vs, channels,this->Textures_VS);
        if (this->CBuffers_VS.empty() && this->CBuffers_fallback_VS != nullptr) {
            ID3D11Buffer* buf = this->CBuffers_fallback_VS->buffer.Get();

            D3D11_BUFFER_DESC desc{};
            buf->GetDesc(&desc);

            const size_t bytes_needed = cb0_vs.size() * sizeof(Vec4);
            const size_t bytes_copy = std::min<size_t>(bytes_needed, desc.ByteWidth);

            if (desc.Usage == D3D11_USAGE_DYNAMIC) {
                D3D11_MAPPED_SUBRESOURCE map{};
                if (SUCCEEDED(pContext->Map(buf, 0, D3D11_MAP_WRITE_DISCARD, 0, &map))) {
                    std::memcpy(map.pData, cb0_vs.data(), bytes_copy);
                    pContext->Unmap(buf, 0);
                }
            }
            else {
                pContext->UpdateSubresource(buf, 0, nullptr, cb0_vs.data(), 0, 0);
            }
            pContext->VSSetConstantBuffers(UINT(this->vsCBSlots_fallback), 1, &buf);
        }
    }
    // Bind all technique constant buffers (your existing behavior)
    if (!this->CBuffers_VS.empty()) {
        for (size_t i = 0; i < this->CBuffers_VS.size(); ++i) {
            ID3D11Buffer* b = this->CBuffers_VS[i]->buffer.Get();
            pContext->VSSetConstantBuffers(UINT(i), 1, &b);
        }
    }


    // Bind scopes explicitly by bit, matching enum order (index == bit position)
    if (TfxScope::has(used, TfxScope::FRAME)) { scopes[0].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::VIEW)) { scopes[1].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::RIGID_MODEL)) { scopes[2].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::EDITOR_MESH)) { scopes[3].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::EDITOR_TERRAIN)) { scopes[4].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_VIEW)) { scopes[5].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_OBJECT)) { scopes[6].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SKINNING)) { scopes[7].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SPEEDTREE)) { scopes[8].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CHUNK_MODEL)) { scopes[9].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::DECAL)) { scopes[10].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::INSTANCES)) { scopes[11].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SPEEDTREE_LOD_DRAWCALL_DATA)) { scopes[12].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TRANSPARENT)) { scopes[13].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TRANSPARENT_ADVANCED)) { scopes[14].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::SDSM_BIAS_AND_SCALE_TEXTURES)) { scopes[15].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::TERRAIN)) { scopes[16].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::POSTPROCESS)) { scopes[17].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_BITMAP)) { scopes[18].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_STANDARD)) { scopes[19].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::UI_FONT)) { scopes[20].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUI_HUD)) { scopes[21].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PARTICLE_TRANSFORMS)) { scopes[22].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PARTICLE_LOCATION_METADATA)) { scopes[23].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::CUBEMAP_VOLUME)) { scopes[24].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_PLATED_TEXTURES)) { scopes[25].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_0)) { scopes[26].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_1)) { scopes[27].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_2)) { scopes[28].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_DECAL)) { scopes[29].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GENERIC_ARRAY)) { scopes[30].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_SKIN)) { scopes[31].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_LIPS)) { scopes[32].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_HAIR)) { scopes[33].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_0_MASK)) { scopes[34].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_0_MATERIAL)) { scopes[35].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_1_MASK)) { scopes[36].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_FACIAL_LAYER_1_MATERIAL)) { scopes[37].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::PLAYER_CENTERED_CASCADED_GRID)) { scopes[38].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::GEAR_DYE_012)) { scopes[39].second.Bind(pContext); }
    if (TfxScope::has(used, TfxScope::COLOR_GRADING_UBERSHADER)) { scopes[40].second.Bind(pContext); }


    return true;
}