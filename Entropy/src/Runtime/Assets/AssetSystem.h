#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <cstdint>

#include "Runtime/Threading/ThreadPool.h"
#include "Runtime/Threading/MainThreadQueue.h"
#include "AssetCache.h"
#include "AssetHandle.h"
#include "Technique.h"           // defines types in namespace EntropyAssets
#include "StaticMesh.h"
#include "RuntimeAssetRegistry.h"
#include "TigerEngine/tag.h"
#include "TigerEngine/Technique/technique.h"
#include "TigerEngine/Technique/texture.h"
#include "Renderer/Loaders/TextureLoader.h"
#include "Sampler.h"
#include "TigerEngine/Technique/tfx/tfx.h"
#include "TigerEngine/Technique/tfx/tfx_eval.h"

struct BufferSRVMeta {
    enum class Kind { Structured, Raw, Typed } kind = Kind::Structured;

    // Structured
    UINT structureByteStride = 0;   // sizeof(T)
    UINT numElements = 0;   // element count

    // Raw (ByteAddressBuffer): byte size must be multiple of 4. Num elements = bytes/4 for the SRV.
    bool allowUAV = false;          // optional (if you also want a UAV later)

    // Typed (Buffer<float4> etc.)
    DXGI_FORMAT typedFormat = DXGI_FORMAT_UNKNOWN; // e.g. DXGI_FORMAT_R32G32B32A32_FLOAT
    UINT bytesPerElement = 0;                   // must match typedFormat (e.g. 16 for RGBA32F)
};


using Microsoft::WRL::ComPtr;

class AssetSystem {
public:
    AssetSystem(ID3D11Device* device,
        ThreadPool& pool,
        MainThreadQueue& mainThread,
        RuntimeAssetRegistry* registry);

    // Buffers
    AssetHandle<ID3D11Buffer> EnqueueBuffer(uint32_t id);

    // Shaders
    AssetHandle<EntropyAssets::VertexShader>   EnqueueVertexShader(uint32_t id);
    AssetHandle<EntropyAssets::PixelShader>    EnqueuePixelShader(uint32_t id);
    AssetHandle<EntropyAssets::ComputeShader>  EnqueueComputeShader(uint32_t id);
    AssetHandle<EntropyAssets::GeometryShader> EnqueueGeometryShader(uint32_t id);
    AssetHandle<EntropyAssets::HullShader>     EnqueueHullShader(uint32_t id);
    AssetHandle<EntropyAssets::DomainShader>   EnqueueDomainShader(uint32_t id);

    // Textures / Samplers / CBuffers
    AssetHandle<EntropyAssets::Texture2DRes> EnqueueTexture(uint32_t id);
    AssetHandle<EntropyAssets::Texture3DRes> Enqueue3DTexture(uint32_t id);
    AssetHandle<EntropyAssets::SamplerRes>   EnqueueSampler(uint32_t id);
    AssetHandle<EntropyAssets::CBufferRes>   EnqueueCBuffer(uint32_t id);

    AssetHandle<EntropyAssets::BufferSRVRes>
        EnqueueBufferSRV(uint32_t id, const BufferSRVMeta& meta);

    // Technique bundle
    std::shared_future<std::shared_ptr<EntropyAssets::Technique>>
        EnqueueTechnique(TagHash techniqueId);

    using InputLayoutProvider =
        std::function<void(uint32_t /*shaderId*/,
            std::vector<D3D11_INPUT_ELEMENT_DESC>& /*descs*/,
            std::vector<std::string>& /*semanticStorage*/)>;

    void SetInputLayoutProvider(InputLayoutProvider fn) { layoutProvider_ = std::move(fn); }

private:
    // ---------------- members ----------------
    ID3D11Device* device_ = nullptr;            // not owned
    ThreadPool& pool_;
    MainThreadQueue& mainThread_;
    RuntimeAssetRegistry* R_ = nullptr;         // not owned

    // Caches (dedupe by ID)
    AssetCache<ID3D11Buffer>                  bufferCache_;
    AssetCache<EntropyAssets::VertexShader>   vsCache_;
    AssetCache<EntropyAssets::PixelShader>    psCache_;
    AssetCache<EntropyAssets::ComputeShader>  csCache_;
    AssetCache<EntropyAssets::GeometryShader> gsCache_;
    AssetCache<EntropyAssets::HullShader>     hsCache_;
    AssetCache<EntropyAssets::DomainShader>   dsCache_;
    AssetCache<EntropyAssets::Texture2DRes>   texCache_;
    AssetCache<EntropyAssets::Texture3DRes>   texCache3D_;
    AssetCache<EntropyAssets::SamplerRes>     sampCache_;
    AssetCache<EntropyAssets::CBufferRes>     cbCache_;
    AssetCache<EntropyAssets::BufferSRVRes> bufSrvCache_;

    // ---------------- helpers ----------------
    std::shared_ptr<ID3D11Buffer> createBuffer_(const BufferPayload& p);
    std::shared_ptr<EntropyAssets::CBufferRes> createCBufferFromRaw_(const void* bytes, UINT sizeBytes);

    template<typename TShaderIface>
    void createShader_(ID3D11Device* dev, const void* bc, size_t bcSize, ComPtr<TShaderIface>& out);
    std::shared_ptr<EntropyAssets::VertexShader>
        createVertexShader_(uint32_t id, const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::PixelShader>
        createPixelShader_(const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::BufferSRVRes>
        createBufferSRV_(const BufferPayload& p, const BufferSRVMeta& meta);

    std::shared_ptr<EntropyAssets::VertexShader> createVS_(const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::Texture2DRes> createTexture_(const Texture2DPayload& p);
    std::shared_ptr<EntropyAssets::SamplerRes>   createSampler_(const D3D11_SAMPLER_DESC& d);
    std::shared_ptr<EntropyAssets::CBufferRes>   createCBuffer_(UINT byteSize, const void* init);
    std::shared_ptr<EntropyAssets::Texture3DRes> createTexture3D_(const Texture3DPayload& p);
  

    InputLayoutProvider  layoutProvider_; // optional hook
};
