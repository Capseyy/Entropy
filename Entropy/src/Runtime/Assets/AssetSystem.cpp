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
	printf("EnqueueBuffer ID %0x8\n", id);
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
	printf("EnqueueVertexShader ID %0x8\n", id);
    auto fut = pool_.Submit([=] {
        return vsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            return createVS_(payload);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::VertexShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::PixelShader> AssetSystem::EnqueuePixelShader(uint32_t id)
{
	printf("EnqueuePixelShader ID %0x8\n", id);
    auto fut = pool_.Submit([=] {
        return psCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            ComPtr<ID3D11PixelShader> ps;
            createShader_<ID3D11PixelShader>(device_, payload.bytecode.data(), payload.bytecode.size(), ps);
            std::shared_ptr<EntropyAssets::PixelShader> out(new EntropyAssets::PixelShader());
            out->ps = ps; return out;
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
std::shared_ptr<EntropyAssets::Texture2DRes> AssetSystem::createTexture_(const TexturePayload& p)
{
    ComPtr<ID3D11Resource> res;
    ComPtr<ID3D11ShaderResourceView> srv;

#if defined(ENTROPY_WITH_DIRECTXTK)
    if (p.kind == TexturePayload::Kind::DDS) {
        HRESULT hr = DirectX::CreateDDSTextureFromMemory(device_, p.data.data(), p.data.size(), res.GetAddressOf(), srv.GetAddressOf(), 0);
        if (FAILED(hr)) throw std::runtime_error("CreateDDSTextureFromMemory failed");
    }
    else if (p.kind == TexturePayload::Kind::WIC) {
        HRESULT hr = DirectX::CreateWICTextureFromMemory(device_, p.data.data(), p.data.size(), res.GetAddressOf(), srv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateWICTextureFromMemory failed");
    }
    else
#endif
    {
        if (p.kind != TexturePayload::Kind::RAWRGBA8)
            throw std::runtime_error("Texture loader: only RAWRGBA8 supported without DirectXTK");

        D3D11_TEXTURE2D_DESC td;
        td.Width = p.w; td.Height = p.h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
        td.SampleDesc.Count = 1; td.SampleDesc.Quality = 0;
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        td.CPUAccessFlags = 0; td.MiscFlags = 0;

        D3D11_SUBRESOURCE_DATA srd;
        srd.pSysMem = p.data.data();
        srd.SysMemPitch = p.pitch ? p.pitch : (p.w * 4);
        srd.SysMemSlicePitch = 0;

        ComPtr<ID3D11Texture2D> tex;
        HRESULT hr = device_->CreateTexture2D(&td, &srd, &tex);
        if (FAILED(hr)) throw std::runtime_error("CreateTexture2D failed");

        D3D11_SHADER_RESOURCE_VIEW_DESC sd;
        sd.Format = td.Format;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;

        hr = device_->CreateShaderResourceView(tex.Get(), &sd, &srv);
        if (FAILED(hr)) throw std::runtime_error("CreateShaderResourceView failed");

        res = tex;
    }

    ComPtr<ID3D11Texture2D> tex2d;
    res.As(&tex2d);

    std::shared_ptr<EntropyAssets::Texture2DRes> out(new EntropyAssets::Texture2DRes());
    out->tex = tex2d; out->srv = srv;
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
    printf("EnqueueTechnique: %08X\n", id);

    static AssetCache<EntropyAssets::Technique> techCache;
    auto valid = [](uint32_t x) { return x != 0u && x != 0xFFFFFFFFu; };

    return techCache.GetOrLoad(id, [=] {
        // 1) Parse your technique blob
        if (!techniqueId.data || techniqueId.size == 0) {
            auto empty = std::make_shared<EntropyAssets::Technique>();
            empty->id = id;
            return empty;
        }
        STechnique Tfx = bin::parse<STechnique>(techniqueId.data, techniqueId.size, bin::Endian::Little);

        // 2) Helpers: ensure shader is registered from tag bytes, then enqueue creation
        auto ensureRegistered = [this](uint32_t shaderId) {
            if (!R_->HasShader(shaderId)) {
                auto sTag = TagHash(shaderId);
                if (!sTag.data || sTag.size == 0) return false;

                ShaderPayload sp{};
                sp.bytecode.assign(
                    static_cast<const uint8_t*>(sTag.data),
                    static_cast<const uint8_t*>(sTag.data) + sTag.size
                );
                // If you have input-layout metadata, fill sp.input here
                R_->RegisterShader(shaderId, std::move(sp));
            }
            return true;
            };

        // 3) Kick all stages in parallel (only if a valid shader id exists)
        std::shared_future<std::shared_ptr<EntropyAssets::VertexShader>>   fVS;
        std::shared_future<std::shared_ptr<EntropyAssets::PixelShader>>    fPS;
        std::shared_future<std::shared_ptr<EntropyAssets::GeometryShader>> fGS;
        std::shared_future<std::shared_ptr<EntropyAssets::HullShader>>     fHS;
        std::shared_future<std::shared_ptr<EntropyAssets::DomainShader>>   fDS;
        std::shared_future<std::shared_ptr<EntropyAssets::ComputeShader>>  fCS;

        // VS
        if (valid(Tfx.VertexShader.ShaderTag.hash) &&
            ensureRegistered(Tfx.VertexShader.ShaderTag.hash))
        {
            fVS = EnqueueVertexShader(Tfx.VertexShader.ShaderTag.hash).future;
        }
        // PS
        if (valid(Tfx.PixelShader.ShaderTag.hash) &&
            ensureRegistered(Tfx.PixelShader.ShaderTag.hash))
        {
            fPS = EnqueuePixelShader(Tfx.PixelShader.ShaderTag.hash).future;
        }
        // GS
        //if (valid(Tfx.GeometryShader.ShaderTag.hash) &&
        //    ensureRegistered(Tfx.GeometryShader.ShaderTag.hash))
        //{
        //    fGS = EnqueueGeometryShader(Tfx.GeometryShader.ShaderTag.hash).future;
        //}
        //// HS (map UnkShader1 if that’s your hull shader)
        //if (valid(Tfx.UnkShader1.ShaderTag.hash) &&
        //    ensureRegistered(Tfx.UnkShader1.ShaderTag.hash))
        //{
        //    fHS = EnqueueHullShader(Tfx.UnkShader1.ShaderTag.hash).future;
        //}
        //// DS (map UnkShader2 if that’s your domain shader)
        //if (valid(Tfx.UnkShader2.ShaderTag.hash) &&
        //    ensureRegistered(Tfx.UnkShader2.ShaderTag.hash))
        //{
        //    fDS = EnqueueDomainShader(Tfx.UnkShader2.ShaderTag.hash).future;
        //}
        //// CS
        //if (valid(Tfx.ComputeShader.ShaderTag.hash) &&
        //    ensureRegistered(Tfx.ComputeShader.ShaderTag.hash))
        //{
        //    fCS = EnqueueComputeShader(Tfx.ComputeShader.ShaderTag.hash).future;
        //}

        // 4) Assemble the Technique (wait now that all futures are in flight)
        auto tech = std::make_shared<EntropyAssets::Technique>();
        tech->id = id;

        try {
            if (fVS.valid()) tech->VS.push_back(fVS.get());
            if (fPS.valid()) tech->PS.push_back(fPS.get());
            if (fGS.valid()) tech->GS.push_back(fGS.get());
            if (fHS.valid()) tech->HS.push_back(fHS.get());
            if (fDS.valid()) tech->DS.push_back(fDS.get());
            if (fCS.valid()) tech->CS.push_back(fCS.get());
        }
        catch (const std::exception& e) {
            std::string msg = "Technique build failed id=" + std::to_string(id) + " : " + e.what() + "\n";
            OutputDebugStringA(msg.c_str());
            // Leave partially filled; renderer can skip parts with missing stages
        }

        // TODO next: textures/samplers/cbuffers (same pattern: ensure register from TagHash -> Enqueue -> get -> push)
        return tech;
        });
}

