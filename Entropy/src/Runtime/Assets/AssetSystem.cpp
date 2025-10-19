#include "AssetSystem.h"
#include <stdexcept>
#include <cassert>

using Microsoft::WRL::ComPtr;

//------------------------------------------------------------------------------
// Shader creation helper (C++11-friendly explicit specializations)
//------------------------------------------------------------------------------
namespace {
    template<typename T>
    inline HRESULT CreateDx11Shader(ID3D11Device*, const void*, size_t, T**) { return E_FAIL; }

    template<> inline HRESULT CreateDx11Shader<ID3D11VertexShader>(ID3D11Device* d, const void* bc, size_t n, ID3D11VertexShader** o) { return d->CreateVertexShader(bc, n, nullptr, o); }
    template<> inline HRESULT CreateDx11Shader<ID3D11PixelShader >(ID3D11Device* d, const void* bc, size_t n, ID3D11PixelShader** o) { return d->CreatePixelShader(bc, n, nullptr, o); }
    template<> inline HRESULT CreateDx11Shader<ID3D11ComputeShader>(ID3D11Device* d, const void* bc, size_t n, ID3D11ComputeShader** o) { return d->CreateComputeShader(bc, n, nullptr, o); }
    template<> inline HRESULT CreateDx11Shader<ID3D11GeometryShader>(ID3D11Device* d, const void* bc, size_t n, ID3D11GeometryShader** o) { return d->CreateGeometryShader(bc, n, nullptr, o); }
    template<> inline HRESULT CreateDx11Shader<ID3D11HullShader>(ID3D11Device* d, const void* bc, size_t n, ID3D11HullShader** o) { return d->CreateHullShader(bc, n, nullptr, o); }
    template<> inline HRESULT CreateDx11Shader<ID3D11DomainShader>(ID3D11Device* d, const void* bc, size_t n, ID3D11DomainShader** o) { return d->CreateDomainShader(bc, n, nullptr, o); }
}

//------------------------------------------------------------------------------
// ctor
//------------------------------------------------------------------------------
AssetSystem::AssetSystem(ID3D11Device* device,
    ThreadPool& pool,
    MainThreadQueue& mainThread,
    RuntimeAssetRegistry* registry)
    : device_(device), pool_(pool), mainThread_(mainThread), R_(registry)
{
    assert(device_ && "AssetSystem requires a valid ID3D11Device*");
}

//------------------------------------------------------------------------------
// Buffers
//------------------------------------------------------------------------------
AssetHandle<ID3D11Buffer> AssetSystem::EnqueueBuffer(uint32_t id)
{
	//printf("EnqueueBuffer ID %0x8\n", id);
    auto fut = pool_.Submit([=] {
        return bufferCache_.GetOrLoad(id, [&] {
            BufferPayload payload = R_->GetBuffer(id);
			printf("Loading Buffer ID %0x8, size %u bytes\n", id, payload.desc.ByteWidth);
            return createBuffer_(payload);
            }).get();
        }).share();

    AssetHandle<ID3D11Buffer> h; h.future = fut; return h;
}

std::shared_ptr<ID3D11Buffer> AssetSystem::createBuffer_(const BufferPayload& p)
{
    D3D11_SUBRESOURCE_DATA srd;
    srd.pSysMem = p.data.empty() ? nullptr : (const void*)p.data.data();
    srd.SysMemPitch = 0; srd.SysMemSlicePitch = 0;

    ComPtr<ID3D11Buffer> buf;
    HRESULT hr = device_->CreateBuffer(&p.desc, p.data.empty() ? nullptr : &srd, &buf);
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer failed");

    ID3D11Buffer* raw = buf.Detach();
    return std::shared_ptr<ID3D11Buffer>(raw, [](ID3D11Buffer* b) { if (b) b->Release(); });
}

//------------------------------------------------------------------------------
// Shaders
//------------------------------------------------------------------------------
std::shared_ptr<EntropyAssets::VertexShader>
AssetSystem::createVertexShader_(uint32_t id, const ShaderPayload& p)
{
    ComPtr<ID3D11VertexShader> vs;
    HRESULT hr = device_->CreateVertexShader(
        p.bytecode.data(), p.bytecode.size(), nullptr, &vs);
    if (FAILED(hr)) {
        printf("CreateVS hr=0x%08X size=%zu magic=0x%08X\n", (unsigned)hr, p.bytecode.size(),
            p.bytecode.size() >= 4 ? *(const uint32_t*)p.bytecode.data() : 0u);
        throw std::runtime_error("CreateVertexShader failed");
    }
    printf("CreateVS hr=0x%08X size=%zu magic=0x%08X\n", (unsigned)hr, p.bytecode.size(),
        p.bytecode.size() >= 4 ? *(const uint32_t*)p.bytecode.data() : 0u);

    auto out = std::make_shared<EntropyAssets::VertexShader>();
    out->vs = vs;

    // Build an input layout if available
    // Source #1: caller-provided provider (recommended); or
    // Source #2: payload.input (if you already store it).
    std::vector<D3D11_INPUT_ELEMENT_DESC> descs;
    std::vector<std::string> semantics; // holds char buffers backing SemanticName

    if (layoutProvider_) {
        layoutProvider_(id, descs, semantics);
    }
    else if (!p.input.empty()) {
        // If you store them in the payload (with stable backing strings)
        descs = p.input; // NOTE: ensure SemanticName pointers are valid
    }

    if (!descs.empty()) {
        ComPtr<ID3D11InputLayout> il;
        hr = device_->CreateInputLayout(
            descs.data(), static_cast<UINT>(descs.size()),
            p.bytecode.data(), p.bytecode.size(),
            &il);
        if (SUCCEEDED(hr)) {
            out->layout = il;
        }
        else {
            OutputDebugStringA("CreateInputLayout failed; continuing without IL\n");
        }
    }

    return out;
}

std::shared_ptr<EntropyAssets::PixelShader>
AssetSystem::createPixelShader_(const ShaderPayload& p)
{
    ComPtr<ID3D11PixelShader> ps;
    HRESULT hr = device_->CreatePixelShader(
        p.bytecode.data(), p.bytecode.size(), nullptr, &ps);
    if (FAILED(hr)) {
        throw std::runtime_error("CreatePixelShader failed");
    }

    auto out = std::make_shared<EntropyAssets::PixelShader>();
    out->ps = ps;
    return out;
}




template<typename TShaderIface>
void AssetSystem::createShader_(ID3D11Device* dev, const void* bc, size_t bcSize, ComPtr<TShaderIface>& out)
{
    HRESULT hr = CreateDx11Shader<TShaderIface>(dev, bc, bcSize, out.GetAddressOf());
    if (FAILED(hr)) throw std::runtime_error("CreateShader failed");
}


std::shared_ptr<EntropyAssets::VertexShader> AssetSystem::createVS_(const ShaderPayload& p)
{
    ComPtr<ID3D11VertexShader> vs;
    createShader_<ID3D11VertexShader>(device_, p.bytecode.data(), p.bytecode.size(), vs);

    ComPtr<ID3D11InputLayout> layout;
    if (!p.input.empty())
    {
        HRESULT hr = device_->CreateInputLayout(
            p.input.data(), static_cast<UINT>(p.input.size()),
            p.bytecode.data(), static_cast<UINT>(p.bytecode.size()),
            &layout);
        if (FAILED(hr)) throw std::runtime_error("CreateInputLayout failed");
    }

    std::shared_ptr<EntropyAssets::VertexShader> out(new EntropyAssets::VertexShader());
    out->vs = vs; out->layout = layout;
    return out;
}

AssetHandle<EntropyAssets::VertexShader> AssetSystem::EnqueueVertexShader(uint32_t id)
{
	//printf("EnqueueVertexShader ID %0x8\n", id);
    auto fut = pool_.Submit([=] {
        return vsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            return createVertexShader_(id, payload);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::VertexShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::PixelShader> AssetSystem::EnqueuePixelShader(uint32_t id)
{
    //printf("EnqueuePixelShader ID %0x8\n", id);
    auto fut = pool_.Submit([=] {
        return psCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            return createPixelShader_(payload);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::PixelShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::ComputeShader> AssetSystem::EnqueueComputeShader(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return csCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            ComPtr<ID3D11ComputeShader> cs;
            createShader_<ID3D11ComputeShader>(device_, payload.bytecode.data(), payload.bytecode.size(), cs);
            std::shared_ptr<EntropyAssets::ComputeShader> out(new EntropyAssets::ComputeShader());
            out->cs = cs; return out;
            }).get();
        }).share();

    AssetHandle<EntropyAssets::ComputeShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::GeometryShader> AssetSystem::EnqueueGeometryShader(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return gsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            ComPtr<ID3D11GeometryShader> gs;
            createShader_<ID3D11GeometryShader>(device_, payload.bytecode.data(), payload.bytecode.size(), gs);
            std::shared_ptr<EntropyAssets::GeometryShader> out(new EntropyAssets::GeometryShader());
            out->gs = gs; return out;
            }).get();
        }).share();

    AssetHandle<EntropyAssets::GeometryShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::HullShader> AssetSystem::EnqueueHullShader(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return hsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            ComPtr<ID3D11HullShader> hs;
            createShader_<ID3D11HullShader>(device_, payload.bytecode.data(), payload.bytecode.size(), hs);
            std::shared_ptr<EntropyAssets::HullShader> out(new EntropyAssets::HullShader());
            out->hs = hs; return out;
            }).get();
        }).share();

    AssetHandle<EntropyAssets::HullShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::DomainShader> AssetSystem::EnqueueDomainShader(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return dsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            ComPtr<ID3D11DomainShader> ds;
            createShader_<ID3D11DomainShader>(device_, payload.bytecode.data(), payload.bytecode.size(), ds);
            std::shared_ptr<EntropyAssets::DomainShader> out(new EntropyAssets::DomainShader());
            out->ds = ds; return out;
            }).get();
        }).share();

    AssetHandle<EntropyAssets::DomainShader> h; h.future = fut; return h;
}

//------------------------------------------------------------------------------
// Textures / Samplers / CBuffers
//------------------------------------------------------------------------------
std::shared_ptr<EntropyAssets::Texture2DRes>
AssetSystem::createTexture_(const TexturePayload& p)
{
    using Microsoft::WRL::ComPtr;

    // Validate & create texture
    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = device_->CreateTexture2D(&p.desc,
        p.subresources.empty() ? nullptr : p.subresources.data(), &tex);
    if (FAILED(hr)) throw std::runtime_error("CreateTexture2D failed");

    // SRV format (typed if the texture is typeless)
    auto ToTypedForSRV = [](DXGI_FORMAT f)->DXGI_FORMAT {
        switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
        case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
        case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
        case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
        case DXGI_FORMAT_BC4_TYPELESS:      return DXGI_FORMAT_BC4_UNORM;
        case DXGI_FORMAT_BC5_TYPELESS:      return DXGI_FORMAT_BC5_UNORM;
        default:                            return f;
        }
        };

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = ToTypedForSRV(p.desc.Format);

    const bool isCube = (p.desc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;
    if (isCube) {
        const UINT cubeCount = p.desc.ArraySize / 6;
        sd.ViewDimension = (cubeCount > 1) ? D3D11_SRV_DIMENSION_TEXTURECUBEARRAY
            : D3D11_SRV_DIMENSION_TEXTURECUBE;
        if (cubeCount > 1) {
            sd.TextureCubeArray.MostDetailedMip = 0;
            sd.TextureCubeArray.MipLevels = p.desc.MipLevels;
            sd.TextureCubeArray.First2DArrayFace = 0;
            sd.TextureCubeArray.NumCubes = cubeCount;
        }
        else {
            sd.TextureCube.MostDetailedMip = 0;
            sd.TextureCube.MipLevels = p.desc.MipLevels;
        }
    }
    else if (p.desc.ArraySize > 1) {
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
        sd.Texture2DArray.MostDetailedMip = 0;
        sd.Texture2DArray.MipLevels = p.desc.MipLevels;
        sd.Texture2DArray.FirstArraySlice = 0;
        sd.Texture2DArray.ArraySize = p.desc.ArraySize;
    }
    else {
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = p.desc.MipLevels;
    }

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device_->CreateShaderResourceView(tex.Get(), &sd, &srv);
    if (FAILED(hr)) throw std::runtime_error("CreateShaderResourceView failed");

    auto out = std::make_shared<EntropyAssets::Texture2DRes>();
    out->srv = srv;
    return out;
}


AssetHandle<EntropyAssets::Texture2DRes> AssetSystem::EnqueueTexture(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return texCache_.GetOrLoad(id, [&] {
            TexturePayload payload = R_->GetTexture(id);
            return createTexture_(payload);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::Texture2DRes> h; h.future = fut; return h;
}

std::shared_ptr<EntropyAssets::SamplerRes> AssetSystem::createSampler_(const D3D11_SAMPLER_DESC& d)
{
    ComPtr<ID3D11SamplerState> s;
    HRESULT hr = device_->CreateSamplerState(&d, &s);
    if (FAILED(hr)) throw std::runtime_error("CreateSamplerState failed");

    std::shared_ptr<EntropyAssets::SamplerRes> out(new EntropyAssets::SamplerRes());
    out->sampler = s;
    return out;
}

AssetHandle<EntropyAssets::SamplerRes> AssetSystem::EnqueueSampler(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return sampCache_.GetOrLoad(id, [&] {
            D3D11_SAMPLER_DESC desc = R_->GetSampler(id);
            return createSampler_(desc);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::SamplerRes> h; h.future = fut; return h;
}

std::shared_ptr<EntropyAssets::CBufferRes> AssetSystem::createCBuffer_(UINT byteSize, const void* init)
{
    D3D11_BUFFER_DESC bd;
    bd.Usage = D3D11_USAGE_DEFAULT;
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.CPUAccessFlags = 0;
    bd.MiscFlags = 0;
    bd.StructureByteStride = 0;
    bd.ByteWidth = (byteSize + 15u) & ~15u; // 16-byte align

    D3D11_SUBRESOURCE_DATA srd;
    srd.pSysMem = init; srd.SysMemPitch = 0; srd.SysMemSlicePitch = 0;

    ComPtr<ID3D11Buffer> buf;
    HRESULT hr = device_->CreateBuffer(&bd, init ? &srd : nullptr, &buf);
    if (FAILED(hr)) throw std::runtime_error("Create constant buffer failed");

    std::shared_ptr<EntropyAssets::CBufferRes> out(new EntropyAssets::CBufferRes());
    out->buffer = buf; out->size = bd.ByteWidth;
    return out;
}

AssetHandle<EntropyAssets::CBufferRes> AssetSystem::EnqueueCBuffer(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return cbCache_.GetOrLoad(id, [&] {
            CBufferMeta meta = R_->GetCBuffer(id);
            const void* init = meta.initial.empty() ? nullptr : (const void*)meta.initial.data();
            return createCBuffer_(meta.byteSize, init);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::CBufferRes> h; h.future = fut; return h;
}

//------------------------------------------------------------------------------
// Techniques
//------------------------------------------------------------------------------
std::shared_future<std::shared_ptr<EntropyAssets::Technique>>
AssetSystem::EnqueueTechnique(TagHash techniqueId)
{
    const uint32_t id = techniqueId.hash;
    printf("[Tech] EnqueueTechnique %08X (tag bytes=%zu)\n", id, (size_t)techniqueId.size);

    static AssetCache<EntropyAssets::Technique> techCache;
    auto valid32 = [](uint32_t x) { return x && x != 0xFFFFFFFFu; };

    return techCache.GetOrLoad(id, [=] {
        // Parse technique
        auto tech = std::make_shared<EntropyAssets::Technique>();
        tech->id = id;

        if (!techniqueId.data || techniqueId.size == 0) {
            printf("[Tech] %08X empty tag\n", id);
            return tech;
        }

        STechnique Tfx = bin::parse<STechnique>(techniqueId.data, techniqueId.size, bin::Endian::Little);

        // -------- Shaders (your existing logic) --------
        const uint32_t vsId = Tfx.VertexShader.ShaderTag.reference;
        const uint32_t psId = Tfx.PixelShader.ShaderTag.reference;
        printf("[Tech] %08X VS id=%08X, PS id=%08X\n", id, vsId, psId);

        auto ensureShaderPayload = [this](uint32_t sid)->bool {
            if (R_->HasShader(sid)) return true;
            auto sTag = TagHash(sid);
            printf("  [Shader] %08X bytes=%zu\n", sid, (size_t)sTag.size);
            if (!sTag.data || sTag.size == 0) return false;

            ShaderPayload sp{};
            sp.bytecode.assign(
                static_cast<const uint8_t*>(sTag.data),
                static_cast<const uint8_t*>(sTag.data) + sTag.size
            );
            R_->RegisterShader(sid, std::move(sp));
            return true;
            };

        std::shared_future<std::shared_ptr<EntropyAssets::VertexShader>> fVS;
        std::shared_future<std::shared_ptr<EntropyAssets::PixelShader>>  fPS;

        if (valid32(vsId) && ensureShaderPayload(vsId))
            fVS = EnqueueVertexShader(vsId).future;
        else
            printf("[Tech] %08X NO VS (id invalid or no bytes)\n", id);

        if (valid32(psId) && ensureShaderPayload(psId))
            fPS = EnqueuePixelShader(psId).future;
        else
            printf("[Tech] %08X NO PS (id invalid or no bytes)\n", id);

        auto ensureTexturePayload = [this](uint32_t texId)->bool {
            if (R_->HasTexture(texId)) return true;

            TagHash texTag(texId);
            if (!texTag.data || texTag.size == 0) {
                printf("  [Tex] %08X missing bytes\n", texId);
                return false;
            }

            auto payload = BuildTexturePayloadFromTag(texTag);
            if (!payload) {
                printf("  [Tex] %08X BuildTexturePayloadFromTag failed\n", texId);
                return false;
            }

            R_->RegisterTexture(texId, std::move(*payload));
            return true;
            };

        using TexFuture = std::shared_future<std::shared_ptr<EntropyAssets::Texture2DRes>>;
        std::vector<std::pair<UINT, TexFuture>> psTexF;
        psTexF.reserve(Tfx.PixelShader.Textures.size());

        printf("[Tech] %08X PS textures: %zu\n", id, Tfx.PixelShader.Textures.size());
        for (const auto& t : Tfx.PixelShader.Textures) {
            const UINT slot = t.TextureIndex;
            const uint32_t texId = t.Texture.tagHash32;// <-- your mapping

            if (!valid32(texId)) {
                printf("  [Tex] slot %u invalid id\n", slot);
                continue;
            }
            if (!ensureTexturePayload(texId))
                continue;

            psTexF.emplace_back(slot, EnqueueTexture(texId).future);
        }

        // -------- Collect everything --------
        if (fVS.valid()) {
            auto vs = fVS.get();
            printf("[Tech] %08X VS ok? %s\n", id, (vs && vs->vs.Get()) ? "YES" : "NO");
            if (vs) tech->VS.push_back(vs);
        }
        if (fPS.valid()) {
            auto ps = fPS.get();
            printf("[Tech] %08X PS ok? %s\n", id, (ps && ps->ps.Get()) ? "YES" : "NO");
            if (ps) tech->PS.push_back(ps);
        }

        tech->Textures.reserve(psTexF.size());
        tech->psTextureSlots.reserve(psTexF.size());
        for (auto& [slot, fut] : psTexF) {
            try {
                if (!fut.valid()) continue;
                auto texRes = fut.get();
                if (texRes) {
                    tech->Textures.push_back(texRes);
                    tech->psTextureSlots.push_back(slot);
                }
            }
            catch (const std::exception& e) {
                printf("[Tech] %08X texture future (slot %u) failed: %s\n", id, slot, e.what());
            }
        }

        return tech;
        });
}


