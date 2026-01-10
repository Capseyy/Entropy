#include "Runtime/Assets/Technique.h"
#undef min
#undef max
#include "TigerEngine/Technique/Tfx/tfx_program.h"
#include "TigerEngine/Technique/Tfx/tfx_eval.h" 
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
    std::optional<uint8_t> depthStencilCombo; 
    std::optional<uint8_t> rasterizer;        
    std::optional<uint8_t> depthBias;         
};

static inline DecodedSelection DecodeStateSelection(uint32_t sel) {
    auto get = [&](int shift)->std::optional<uint8_t> {
        uint8_t v = uint8_t((sel >> shift) & 0xFF);
        return (v & 0x80) ? std::optional<uint8_t>(v & 0x7F) : std::nullopt;
        };
    return {
                    get(0),
        get(8),
               get(16),
                get(24)
    };
}

bool EntropyAssets::Technique::Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes)
{
    uint32_t StateSelection = this->StateSelection;
    uint64_t UsedScopes = this->usedScopes;
    auto used = TfxScope::from_bits_truncate(this->usedScopes);
    ShaderBindingState bindingState{};
    const auto sel = DecodeStateSelection(this->StateSelection);
    if (this->id == 0x80EFC074) {
        int u = 1;
    }
    
    if (sel.blend && *sel.blend < states.blend_states.size() &&
        states.blend_states[*sel.blend]) {
        const float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };
        const UINT sampleMask = 0xFFFFFFFFu;
        
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

    
    ShaderBindingState binds{};
    prog.Evaluate(externs, cb0, this->Textures, &binds, false);
 
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
        std::vector<int> sstack;
        sstack.reserve(16);

        auto ops = ParseAll(this->pixeldata.TFX_Bytecode, false);
        for (const auto& op : ops) {
            switch (op.op) {
            case TfxBytecode::PushSampler: {
                auto d = std::get<PushSamplerData>(op.data);
                sstack.push_back(int(d.index));
                break;
            }
            case TfxBytecode::SetShaderSampler: {
                auto d = std::get<SetShaderBindingData>(op.data);
                
                if (!sstack.empty()) {
                    int idx = sstack.back();
                    sstack.pop_back();
                    if (d.stage == 1) {
                        psBinds.push_back(PsSamplerBind{ UINT(d.slot), UINT(idx) });
                    }
                }
                break;
            }
            default:
                break;
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
            ID3D11ShaderResourceView* srv = this->Textures[i]->Get();
			
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

    
    
    
    for (UINT slot = 0; slot < ShaderBindingState::MaxSlots; ++slot) {
        if (!binds.textures[1][slot]) continue; 

        ID3D11ShaderResourceView* srv = nullptr;
        if (binds.textures[1][slot]) {
            srv = binds.textures[1][slot]->Get();
            
        }

        pContext->PSSetShaderResources(slot, 1, &srv);
    }
        
    

    if (this->vertexdata.TFX_Bytecode.size() != 0 && this->vertexdata.contstant_buffer.hash == 0xffffffff) {
        TfxProgram prog_vs = TfxProgram::FromBytecode(this->vertexdata.TFX_Bytecode,
            this->vertexdata.TFX_Constants, this->id);
        auto& cb0_vs = this->vertexdata.SamplerFallback;
        prog_vs.Evaluate(externs, cb0_vs, this->Textures_VS, nullptr, false);
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
  
    if (!this->CBuffers_VS.empty()) {
        for (size_t i = 0; i < this->CBuffers_VS.size(); ++i) {
            ID3D11Buffer* b = this->CBuffers_VS[i]->buffer.Get();
            pContext->VSSetConstantBuffers(UINT(i), 1, &b);
        }
    }

   


    
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

bool EntropyAssets::Technique::Bind_With_Channels(
    Microsoft::WRL::ComPtr<ID3D11Device>        /*pDevice*/,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext,
    ExternStorage& externs,
    RenderStates& states,
    std::vector<std::pair<std::string, TigerScope>>& scopes,
    std::unordered_map<uint32_t, Vec4> channels)
{

    const auto sel = DecodeStateSelection(this->StateSelection);

    if (sel.blend && *sel.blend < states.blend_states.size() && states.blend_states[*sel.blend]) {
        const float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };
        const UINT sampleMask = 0xFFFFFFFFu;
        pContext->OMSetBlendState(states.blend_states[*sel.blend].Get(), blendFactor, sampleMask);
    }

    if (sel.rasterizer && *sel.rasterizer < states.rasterizer_states.size()) {
        auto& rs = states.rasterizer_states[*sel.rasterizer];
        if (rs) pContext->RSSetState(rs.Get());
    }


    if (!this->VS.empty()) {
        pContext->VSSetShader(this->VS[0]->vs.Get(), nullptr, 0);
    }
    if (!this->PS.empty()) {
        pContext->PSSetShader(this->PS[0]->ps.Get(), nullptr, 0);
    }

    enum class Stage : uint8_t { VS = 0, PS = 1 };

    auto SetCBs = [&](Stage st, UINT slot, UINT count, ID3D11Buffer* const* bufs) {
        if (st == Stage::VS) pContext->VSSetConstantBuffers(slot, count, bufs);
        else                pContext->PSSetConstantBuffers(slot, count, bufs);
        };

    auto SetSRVs = [&](Stage st, UINT slot, UINT count, ID3D11ShaderResourceView* const* srvs) {
        if (st == Stage::VS) pContext->VSSetShaderResources(slot, count, srvs);
        else                pContext->PSSetShaderResources(slot, count, srvs);
        };

    auto SetSamplers = [&](Stage st, UINT slot, UINT count, ID3D11SamplerState* const* samps) {
        if (st == Stage::VS) pContext->VSSetSamplers(slot, count, samps);
        else                pContext->PSSetSamplers(slot, count, samps);
        };

    auto UploadFallbackCB0 = [&](ID3D11Buffer* buf, const std::vector<Vec4>& cb0) {
        if (!buf) return;

        D3D11_BUFFER_DESC desc{};
        buf->GetDesc(&desc);

        const size_t bytes_needed = cb0.size() * sizeof(Vec4);
        const size_t bytes_copy = std::min<size_t>(bytes_needed, desc.ByteWidth);

        if (bytes_copy == 0) return;

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
        };

    struct SamplerBind { UINT slot; UINT sampler_index; };

    auto ParseSamplerBinds = [&](const std::vector<uint8_t>& bytecode, Stage stage) -> std::vector<SamplerBind> {
        std::vector<SamplerBind> out;
        std::vector<int> sstack; sstack.reserve(16);

        auto ops = ParseAll(bytecode, false);
        for (const auto& op : ops) {
            switch (op.op) {
            case TfxBytecode::PushSampler: {
                auto d = std::get<PushSamplerData>(op.data);
                sstack.push_back(int(d.index));
                break;
            }
            case TfxBytecode::SetShaderSampler: {
                auto d = std::get<SetShaderBindingData>(op.data);
                if (sstack.empty()) break;

                const int idx = sstack.back();
                sstack.pop_back();

               
                const uint32_t want = (stage == Stage::VS) ? 0u : 1u;
                if (d.stage == want) {
                    out.push_back(SamplerBind{ UINT(d.slot), UINT(idx) });
                }
                break;
            }
            default: break;
            }
        }
        return out;
        };

   
    auto BindStage = [&](Stage stage, bool trace = false) {
     
        const auto& tfxBytecode = (stage == Stage::PS) ? this->pixeldata.TFX_Bytecode : this->vertexdata.TFX_Bytecode;
        const auto& tfxConstants = (stage == Stage::PS) ? this->pixeldata.TFX_Constants : this->vertexdata.TFX_Constants;

        auto& cb0 = (stage == Stage::PS) ? this->pixeldata.SamplerFallback : this->vertexdata.SamplerFallback;

        auto& textures2D = (stage == Stage::PS) ? this->Textures : this->Textures_VS;
        auto& textures3D = (stage == Stage::PS) ? this->Textures3D : this->Textures3D_VS;
        const auto& slots2D = (stage == Stage::PS) ? this->psTextureSlots : this->vsTextureSlots;
        const auto& slots3D = (stage == Stage::PS) ? this->psTextureSlots3D : this->vsTextureSlots3D;

        auto& samplers = (stage == Stage::PS) ? this->Samplers : this->Samplers_VS;

        auto& cbuffers = (stage == Stage::PS) ? this->CBuffers : this->CBuffers_VS;

        auto* fallbackCB =
            (stage == Stage::PS)
            ? (this->CBuffers_fallback ? this->CBuffers_fallback->buffer.Get() : nullptr)
            : (this->CBuffers_fallback_VS ? this->CBuffers_fallback_VS->buffer.Get() : nullptr);

        const UINT fallbackCBSlot =
            (stage == Stage::PS) ? UINT(this->psCBSlots_fallback) : UINT(this->vsCBSlots_fallback);

        ShaderBindingState binds{};
        TfxProgram prog = TfxProgram::FromBytecode(tfxBytecode, tfxConstants, this->id);
		if (trace)
		    printf("Binding TFX Program ID 0x%08X to %s stage\n", this->id, (stage == Stage::PS) ? "PS" : "VS");
        prog.Evaluate_With_Channels(externs, cb0, channels, textures2D, &binds, trace);

        // --- Constant buffers ---
        if (cbuffers.empty() && fallbackCB) {
            UploadFallbackCB0(fallbackCB, cb0);
            ID3D11Buffer* buf = fallbackCB;
            SetCBs(stage, fallbackCBSlot, 1, &buf);
        }
        else if (!cbuffers.empty()) {
            for (UINT i = 0; i < (UINT)cbuffers.size(); ++i) {
                ID3D11Buffer* b = cbuffers[i]->buffer.Get();
                SetCBs(stage, i, 1, &b);
            }
        }

        const auto samplerBinds = ParseSamplerBinds(tfxBytecode, stage);
        if (!samplerBinds.empty()) {
            for (const auto& b : samplerBinds) {
                if (b.sampler_index < samplers.size() && samplers[b.sampler_index]) {
                    ID3D11SamplerState* s = samplers[b.sampler_index]->sampler.Get();
                    SetSamplers(stage, b.slot, 1, &s);
                }
            }
        }
        else {
            
            for (UINT i = 0; i < (UINT)samplers.size(); ++i) {
                ID3D11SamplerState* s = samplers[i] ? samplers[i]->sampler.Get() : nullptr;
                SetSamplers(stage, i, 1, &s);
            }
        }

      
        if (!textures2D.empty() && !slots2D.empty()) {
            const size_t n = std::min(textures2D.size(), slots2D.size());
            for (size_t i = 0; i < n; ++i) {
                const UINT slot = slots2D[i];
                ID3D11ShaderResourceView* srv = textures2D[i] ? textures2D[i]->Get() : nullptr;
                SetSRVs(stage, slot, 1, &srv);
            }
        }

        
        if (!textures3D.empty() && !slots3D.empty()) {
            const size_t n = std::min(textures3D.size(), slots3D.size());
            for (size_t i = 0; i < n; ++i) {
                const UINT slot = slots3D[i];
                ID3D11ShaderResourceView* srv = textures3D[i] ? textures3D[i]->Get() : nullptr;
                SetSRVs(stage, slot, 1, &srv);
            }
        }

        
        const UINT bindsStageIndex = (stage == Stage::VS) ? 0u : 1u;
        for (UINT slot = 0; slot < ShaderBindingState::MaxSlots; ++slot) {
            if (!binds.textures[bindsStageIndex][slot]) continue;
            ID3D11ShaderResourceView* srv = binds.textures[bindsStageIndex][slot]->Get();
            SetSRVs(stage, slot, 1, &srv);
        }
        };

  /*  if (this->id == 0x81089F5D) {
        BindStage(Stage::PS, true);
        BindStage(Stage::VS, true);
    }
    else */
    {
        BindStage(Stage::PS);
        BindStage(Stage::VS);
    }
    

    
    const auto used = TfxScope::from_bits_truncate(this->usedScopes);


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
