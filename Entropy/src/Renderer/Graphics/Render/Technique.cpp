#include "Runtime/Assets/Technique.h"
#undef min
#undef max
#include "TigerEngine/Technique/Tfx/tfx_program.h"

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
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage externs, RenderStates states)
{
    uint32_t StateSelection = this->StateSelection;


    const auto sel = DecodeStateSelection(this->StateSelection);

    // Blend ----------------------------------------------------------
    if (sel.blend && *sel.blend < states.blend_states.size() &&
        states.blend_states[*sel.blend]) {
        const float blendFactor[4] = { 1.f, 1.f, 1.f, 1.f };
        const UINT sampleMask = 0xFFFFFFFFu;
        //printf("Mapped blend\n");
        pContext->OMSetBlendState(states.blend_states[*sel.blend].Get(), blendFactor, sampleMask);
    }

    if (sel.rasterizer && *sel.rasterizer < 9 &&
        sel.depthBias && *sel.depthBias < 9) {
        auto& rs = states.rasterizer_states[*sel.depthBias][*sel.rasterizer];
        if (rs) {
            //printf("Mapped raster\n");
            pContext->RSSetState(rs.Get());
        }
    }
    if (sel.depthStencilCombo && *sel.depthStencilCombo < states.depth_stencil_states.size()) {
        // Choose primary or "alt" (reverse) depth func variant.
        // Wire this to whatever logic you use to flip GREATER/LESS, etc.
        const bool useAltDepth = false;               // TODO: set this from your engine state
        const UINT stencilRef = 0;                   // TODO: set if your pass needs a non-zero ref

        auto& pair = states.depth_stencil_states[*sel.depthStencilCombo];
        ID3D11DepthStencilState* ds = (useAltDepth ? pair.second : pair.first).Get();
        printf("Mapped stencil\n");
        pContext->OMSetDepthStencilState(ds, stencilRef);
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
        this->pixeldata.TFX_Constants);
    // implement getFloat/getVec4/getMat4 in extern.cpp
    auto& cb0 = this->pixeldata.SamplerFallback; // std::vector<Vec4> used as cb0 backing
    prog.Evaluate(externs, cb0);         // writes float4s to cb0[..] as dictated by bytecode
    if (this->id == 0x80C0D09E) {
        //printf("%s \n", prog.DecompilePretty().c_str());
    }
    // ---- 2) Upload cb0 into a D3D11 constant buffer (slot 0 by convention) ----
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

    return true;
}