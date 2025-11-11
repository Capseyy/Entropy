#pragma once
#include <vector>
#include <unordered_map>
#include <mutex>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <d3d11.h>
#include <string>

// ---------- Payloads ----------
struct BufferPayload {
    D3D11_BUFFER_DESC desc{};
    std::vector<uint8_t> data;
    UINT stride = 0;
	uint32_t id = 0;
};

struct ShaderPayload {
    std::vector<uint8_t> bytecode;
    std::vector<D3D11_INPUT_ELEMENT_DESC> input; // non-empty for VS
};

struct Texture2DPayload{
    D3D11_TEXTURE2D_DESC  desc{};     // used when dim==Tex3D
    std::vector<D3D11_SUBRESOURCE_DATA> subresources; // pointers into `data`
    std::vector<uint8_t> data;                        // owns the pixel bytes
};

struct Texture3DPayload {
    D3D11_TEXTURE3D_DESC desc{};
    std::vector<uint8_t> data;                     // tightly-packed mips [mip0 all Z][mip1 all Z]...
    std::vector<D3D11_SUBRESOURCE_DATA> subresources; // one per mip, pSysMem points into data
};

struct CBufferMeta {
    UINT byteSize = 0;                // we'll align to 16
    std::vector<uint8_t> initial;     // optional
};

// ---------- Technique descriptor ----------
enum class ShaderStage : uint8_t { VS, PS, GS, HS, DS, CS };

struct TechniqueDesc {
    std::optional<uint32_t> vs, ps, gs, hs, ds, cs;
    std::vector<uint32_t> textures;   // SRV IDs (binding order)
    std::vector<uint32_t> samplers;   // sampler IDs (binding order)
    std::vector<uint32_t> cbuffers;   // cbuffer IDs (binding order)
};

// ---------- Runtime registry (thread-safe; single mutex) ----------
class RuntimeAssetRegistry {
public:
    // Register (any thread)
    void RegisterBuffer(uint32_t id, BufferPayload payload);
    void RegisterShader(uint32_t id, ShaderPayload payload);
    void RegisterTexture(uint32_t id, Texture2DPayload payload);
    void Register3DTexture(uint32_t id, Texture3DPayload payload);
    void RegisterSampler(uint32_t id, const D3D11_SAMPLER_DESC& desc);
    void RegisterCBuffer(uint32_t id, CBufferMeta meta);
    void RegisterTechnique(uint32_t techId, TechniqueDesc desc);

    // Lookups (throw std::runtime_error if missing)
    BufferPayload    GetBuffer(uint32_t id) const;
    ShaderPayload    GetShader(uint32_t id) const;
    Texture2DPayload   GetTexture(uint32_t id) const;
    Texture3DPayload   Get3DTexture(uint32_t id) const;
    D3D11_SAMPLER_DESC GetSampler(uint32_t id) const;
    CBufferMeta      GetCBuffer(uint32_t id) const;
    TechniqueDesc    GetTechnique(uint32_t techId) const;

    // Optional predicates
    bool HasBuffer(uint32_t id) const;
    bool HasShader(uint32_t id) const;
    bool HasTexture(uint32_t id) const;
    bool HasSampler(uint32_t id) const;
    bool HasCBuffer(uint32_t id) const;
    bool HasTechnique(uint32_t id) const;
    bool Has3DTexture(uint32_t id) const;

private:
    template<class T>
    static const T& must_find_(const std::unordered_map<uint32_t, T>& m, uint32_t id) {
        auto it = m.find(id);
        if (it == m.end()) {
            char buf[128];
            std::snprintf(buf, sizeof(buf), "RuntimeAssetRegistry: missing id 0x%08X\n", id);
            printf(buf);
            throw std::runtime_error(buf);
        }
        return it->second;
    }



    // Single mutex: simplest + works everywhere
    mutable std::mutex m_;
    std::unordered_map<uint32_t, BufferPayload>      buffers_;
    std::unordered_map<uint32_t, ShaderPayload>      shaders_;
    std::unordered_map<uint32_t, Texture2DPayload>     textures_;
    std::unordered_map<uint32_t, Texture3DPayload>     textures3d_;
    std::unordered_map<uint32_t, D3D11_SAMPLER_DESC> samplers_;
    std::unordered_map<uint32_t, CBufferMeta>        cbuffers_;
    std::unordered_map<uint32_t, TechniqueDesc>      techniques_;
};
