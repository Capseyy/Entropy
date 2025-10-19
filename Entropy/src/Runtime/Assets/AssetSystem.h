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
    AssetHandle<EntropyAssets::SamplerRes>   EnqueueSampler(uint32_t id);
    AssetHandle<EntropyAssets::CBufferRes>   EnqueueCBuffer(uint32_t id);

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
    AssetCache<EntropyAssets::SamplerRes>     sampCache_;
    AssetCache<EntropyAssets::CBufferRes>     cbCache_;

    // ---------------- helpers ----------------
    std::shared_ptr<ID3D11Buffer> createBuffer_(const BufferPayload& p);

    template<typename TShaderIface>
    void createShader_(ID3D11Device* dev, const void* bc, size_t bcSize, ComPtr<TShaderIface>& out);
    std::shared_ptr<EntropyAssets::VertexShader>
        createVertexShader_(uint32_t id, const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::PixelShader>
        createPixelShader_(const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::VertexShader> createVS_(const ShaderPayload& p);

    std::shared_ptr<EntropyAssets::Texture2DRes> createTexture_(const TexturePayload& p);
    std::shared_ptr<EntropyAssets::SamplerRes>   createSampler_(const D3D11_SAMPLER_DESC& d);
    std::shared_ptr<EntropyAssets::CBufferRes>   createCBuffer_(UINT byteSize, const void* init);

    InputLayoutProvider  layoutProvider_; // optional hook
};
