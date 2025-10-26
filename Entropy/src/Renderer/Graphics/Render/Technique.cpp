#include "Runtime/Assets/Technique.h"
#undef min
#undef max
#include "TigerEngine/Technique/Tfx/tfx_program.h"

bool EntropyAssets::Technique::Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage externs)
{
    // ---- 1) Evaluate TFX to fill cb0 (float4 array) ----
    TfxProgram prog = TfxProgram::FromBytecode(this->pixeldata.TFX_Bytecode,
        this->pixeldata.TFX_Constants);
    // implement getFloat/getVec4/getMat4 in extern.cpp
    auto& cb0 = this->pixeldata.SamplerFallback; // std::vector<Vec4> used as cb0 backing
    prog.Evaluate(externs, cb0);         // writes float4s to cb0[..] as dictated by bytecode

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