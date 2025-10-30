#include "AssetSystem.h"
#include <stdexcept>
#include <cassert>
#include <optional> 
using Microsoft::WRL::ComPtr;

#include <string>
#include <sstream>
#include <iomanip>


std::string BytesToHex(const std::vector<uint8_t>& v, char sep = ' ') {
    std::ostringstream oss;
    oss << std::uppercase << std::hex << std::setfill('0');
    for (size_t i = 0; i < v.size(); ++i) {
        oss << std::setw(2) << static_cast<unsigned>(v[i]);
        if (sep && i + 1 < v.size()) oss << sep;
    }
    return oss.str();
}

inline void SetDebugName(ID3D11DeviceChild* obj, const char* name)
{
    if (!obj || !name) return;
    obj->SetPrivateData(WKPDID_D3DDebugObjectName,
        static_cast<UINT>(std::strlen(name)), name);
}

inline void SetDebugNameFmt(ID3D11DeviceChild* obj, const char* fmt, ...)
{
    if (!obj || !fmt) return;
    char buf[128];
    va_list ap; va_start(ap, fmt);
    std::vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    obj->SetPrivateData(WKPDID_D3DDebugObjectName,
        static_cast<UINT>(std::strlen(buf)), buf);
}

std::array<float_t, 4> ToArray(const Vec4& v) {
    return { v.x, v.y, v.z, v.w };
}

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
			//printf("Loading Buffer ID %0x8, size %u bytes\n", id, payload.desc.ByteWidth);
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
    return std::shared_ptr<ID3D11Buffer>(raw, [](ID3D11Buffer* b) { ; });
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
    SetDebugNameFmt(vs.Get(), "VS_%08X", id);
    //printf("CreateVS hr=0x%08X size=%zu magic=0x%08X\n", (unsigned)hr, p.bytecode.size(),
     //   p.bytecode.size() >= 4 ? *(const uint32_t*)p.bytecode.data() : 0u);

    auto out = std::make_shared<EntropyAssets::VertexShader>();
    out->vs = vs;

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
    //SetDebugNameFmt(ps.Get(), "VS_%08X", );
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

AssetHandle<EntropyAssets::BufferSRVRes>
AssetSystem::EnqueueBufferSRV(uint32_t id, const BufferSRVMeta& meta)
{
    auto fut = pool_.Submit([=] {
        return bufSrvCache_.GetOrLoad(id, [&] {
            // Pull bytes from registry (same as EnqueueBuffer)
            BufferPayload payload = R_->GetBuffer(id);
            return createBufferSRV_(payload, meta);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::BufferSRVRes> h; h.future = fut; return h;
}

//------------------------------------------------------------------------------
// Textures / Samplers / CBuffers
//------------------------------------------------------------------------------


std::shared_ptr<EntropyAssets::Texture3DRes>
AssetSystem::createTexture3D_(const Texture3DPayload& p)
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID3D11Texture3D> tex;
    HRESULT hr = device_->CreateTexture3D(
        &p.desc, p.subresources.empty() ? nullptr : p.subresources.data(), &tex);
    if (FAILED(hr)) throw std::runtime_error("CreateTexture3D failed");

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = TypelessToTypedSRV3D(p.desc.Format);
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    sd.Texture3D.MostDetailedMip = 0;
    sd.Texture3D.MipLevels       = p.desc.MipLevels;

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device_->CreateShaderResourceView(tex.Get(), &sd, &srv);
    if (FAILED(hr)) throw std::runtime_error("CreateSRV3D failed");

    auto out = std::make_shared<EntropyAssets::Texture3DRes>();
    out->srv = srv;
    return out;
}


std::shared_ptr<EntropyAssets::Texture2DRes>
AssetSystem::createTexture_(const Texture2DPayload& p)
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

AssetHandle<EntropyAssets::Texture3DRes> AssetSystem::Enqueue3DTexture(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return texCache3D_.GetOrLoad(id, [&] {
            Texture3DPayload payload = R_->Get3DTexture(id);
            return createTexture3D_(payload);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::Texture3DRes> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::Texture2DRes> AssetSystem::EnqueueTexture(uint32_t id)
{
    auto fut = pool_.Submit([=] {
        return texCache_.GetOrLoad(id, [&] {
            Texture2DPayload payload = R_->GetTexture(id);
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

std::shared_ptr<EntropyAssets::BufferSRVRes>
AssetSystem::createBufferSRV_(const BufferPayload& p, const BufferSRVMeta& meta)
{
    using Microsoft::WRL::ComPtr;

    D3D11_BUFFER_DESC bd = {};
    bd.ByteWidth = p.desc.ByteWidth;
    bd.Usage = D3D11_USAGE_DEFAULT;     // or DYNAMIC if you Map/Write
    bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags = 0;                       // DYNAMIC -> D3D11_CPU_ACCESS_WRITE
    bd.MiscFlags = 0;                       // leave 

    UINT numElements = p.desc.ByteWidth / p.stride;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Format = (p.stride == 1)
        ? DXGI_FORMAT_R8_UNORM
        : DXGI_FORMAT_R8G8B8A8_UNORM;          // 4 bytes -> float4(unorm) in HLSL
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Buffer.FirstElement = 0;
    sd.Buffer.NumElements = numElements;

    

    // Create buffer with initial data from payload (if any)
    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = p.data.empty() ? nullptr : (const void*)p.data.data();

    ComPtr<ID3D11Buffer> buf;
    HRESULT hr = device_->CreateBuffer(&bd, p.data.empty() ? nullptr : &srd, &buf);
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer (SRV) failed");

    ComPtr<ID3D11ShaderResourceView> srv;
    hr = device_->CreateShaderResourceView(buf.Get(), &sd, &srv);
    if (FAILED(hr)) throw std::runtime_error("CreateShaderResourceView (buffer) failed");

    auto out = std::make_shared<EntropyAssets::BufferSRVRes>();
    out->buffer = buf;
    out->srv = srv;
    return out;
}

static inline UINT Align162(UINT x) { return (x + 15u) & ~15u; }

std::shared_ptr<EntropyAssets::CBufferRes>
AssetSystem::createCBufferFromRaw_(const void* bytes, UINT sizeBytes)
{
    if (!bytes || sizeBytes == 0) return {};

    const UINT byteWidth = Align162(sizeBytes);

    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = byteWidth;                    // must be multiple of 16
    bd.Usage = D3D11_USAGE_DYNAMIC;            // fallback is static
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
 
    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = bytes;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buf;
    HRESULT hr = device_->CreateBuffer(&bd, &srd, &buf);
    if (FAILED(hr)) throw std::runtime_error("Create fallback cbuffer failed");

    auto out = std::make_shared<EntropyAssets::CBufferRes>();
    out->buffer = buf;
    out->size = byteWidth;
    return out;
}

static inline UINT Align16(UINT n) { return (n + 15u) & ~15u; }
//------------------------------------------------------------------------------
// Techniques
//------------------------------------------------------------------------------
std::shared_future<std::shared_ptr<EntropyAssets::Technique>>
AssetSystem::EnqueueTechnique(TagHash techniqueId)
{
    const uint32_t id = techniqueId.hash;
    //printf("[Tech] EnqueueTechnique %08X (tag bytes=%zu)\n", id, (size_t)techniqueId.size);

    static AssetCache<EntropyAssets::Technique> techCache;

    return techCache.GetOrLoad(id, [=] {
        auto tech = std::make_shared<EntropyAssets::Technique>();
        tech->id = id;

        if (!techniqueId.data || techniqueId.size == 0) {
            printf("[Tech] %08X empty tag\n", id);
            return tech;
        }

        STechnique Tfx = bin::parse<STechnique>(techniqueId.data, techniqueId.size, bin::Endian::Little);
        printf("Starting Eval for MAT %08X \n", id);
        //auto lines = Disassemble(Tfx.PixelShader.TFX_Bytecode, Tfx.PixelShader.TFX_Constants);
        //std::vector<Vec4> cb0(64, Vec4::ZERO());   // output constant buffer
        //std::array<Vec4, 16> temps{};               // 16 temp slots

        //// 4) Externs provider
        //ExternStorage externs;

        //// 5) Run
        //EvaluateExpressionEoF(ops, externs, cb0, &Tfx.PixelShader.TFX_Constants, temps);


        // --------- Shaders (your existing) ----------
        const uint32_t vsId = Tfx.VertexShader.ShaderTag.reference;
        const uint32_t psId = Tfx.PixelShader.ShaderTag.reference;
       
        auto ensureShaderPayload = [this](uint32_t sid)->bool {
            if (R_->HasShader(sid)) return true;
            auto sTag = TagHash(sid);
            if (!sTag.data || sTag.size == 0) return false;
            ShaderPayload sp{};
            sp.bytecode.assign((const uint8_t*)sTag.data, (const uint8_t*)sTag.data + sTag.size);
            R_->RegisterShader(sid, std::move(sp));
            return true;
            };

        auto ensureSamplerPayload = [this](uint32_t sampId) -> bool
            {
                if (R_->HasSampler(sampId)) return true;

                // Try to build from tag bytes if you have a sampler blob
                TagHash sTag(sampId);
                std::optional<D3D11_SAMPLER_DESC> descOpt;
                if (sTag.data && sTag.size) {
                    // NOTE: Do NOT define BuildSamplerDescFromTag here; just call it.
                    descOpt = BuildSamplerDescFromTag(sTag);
                }

                // Fallback default if no blob / parse failed
                D3D11_SAMPLER_DESC d{};
                if (descOpt) {
                    d = *descOpt;
                }
                else {
                    d.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
                    d.AddressU = d.AddressV = d.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
                    d.MipLODBias = 0.0f;
                    d.MaxAnisotropy = 1;
                    d.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
                    ZeroMemory(d.BorderColor, sizeof(d.BorderColor));
                    d.MinLOD = 0.0f;
                    d.MaxLOD = D3D11_FLOAT32_MAX;
                }

                R_->RegisterSampler(sampId, d);
                return true;
            };
        auto ensureCBufferPayload = [this](uint32_t cbId, TagHash cbTag) -> bool
            {
                if (R_->HasCBuffer(cbId)) return true;

                CBufferMeta meta{};
                if (cbTag.data && cbTag.size) {
                    meta.byteSize = Align16(static_cast<UINT>(cbTag.size));
                    meta.initial.resize(static_cast<size_t>(cbTag.size));
                    std::memcpy(meta.initial.data(), cbTag.data, static_cast<size_t>(cbTag.size));
                    // any padding up to byteSize stays zeroed
                }
                else {
                    // no blob? register an empty 16-byte cbuffer
                    meta.byteSize = 16u;
                }

                R_->RegisterCBuffer(cbId, std::move(meta));
                return true;
            };
        std::shared_future<std::shared_ptr<EntropyAssets::VertexShader>> fVS;
        std::shared_future<std::shared_ptr<EntropyAssets::PixelShader>>  fPS;
        if (ensureShaderPayload(vsId)) fVS = EnqueueVertexShader(vsId).future;
        if (ensureShaderPayload(psId)) fPS = EnqueuePixelShader(psId).future;

        // --------- Textures (your existing) ----------
        using TexFuture = std::shared_future<std::shared_ptr<EntropyAssets::Texture2DRes>>;
        using TexFuture3D = std::shared_future<std::shared_ptr<EntropyAssets::Texture3DRes>>;
        std::vector<std::pair<UINT, TexFuture>> psTexF;
        std::vector<std::pair<UINT, TexFuture3D>> psTexF3D;
        psTexF.reserve(Tfx.PixelShader.Textures.size());
        for (const auto& t : Tfx.PixelShader.Textures) {
            const UINT slot = t.TextureIndex;
            const uint32_t texId = t.Texture.tagHash32;   // your mapping
            TagHash texTag(texId);
            if (texTag.sub_type == 3) {
                printf("Found 3s tex");
                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTexture3DPayloadFromTag(texTag)) {
                        R_->Register3DTexture(texId, std::move(*payload));
                    }
                    else {
                        continue;
                    }
                }
                psTexF3D.emplace_back(slot, Enqueue3DTexture(texId).future);
            }
            else {
                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTexturePayloadFromTag(texTag)) {
                        R_->RegisterTexture(texId, std::move(*payload));
                    }
                    else {
                        continue;
                    }
                }
                psTexF.emplace_back(slot, EnqueueTexture(texId).future);
            }
            // ensure + enqueue (your helpers)
            
            
        }

        using SampFuture = std::shared_future<std::shared_ptr<EntropyAssets::SamplerRes>>;
        std::vector<std::pair<UINT, SampFuture>> psSampF;
        psSampF.reserve(Tfx.PixelShader.Samplers.size());

        for (size_t i = 0; i < Tfx.PixelShader.Samplers.size(); ++i)
        {
            const auto& s = Tfx.PixelShader.Samplers[i];
            const uint32_t id = s.sampler.reference;                      // sampler id (32-bit)

            // prefer explicit slot if present; otherwise index
            const UINT slot = i;

            // **critical**: put a payload into the registry BEFORE enqueue
            if (!ensureSamplerPayload(id)) continue;

            psSampF.emplace_back(slot, EnqueueSampler(id).future);
        }

        // ---------- PS CONSTANT BUFFER ----------
        std::shared_future<std::shared_ptr<EntropyAssets::CBufferRes>> fPSCB;
        UINT psCBSlot = 0;

        const uint32_t cbId = Tfx.PixelShader.contstant_buffer.reference;
        if (cbId != 0u && cbId != 0xFFFFFFFFu) {
            psCBSlot = static_cast<UINT>(
                Tfx.PixelShader.constant_buffer_slot >= 0 ? Tfx.PixelShader.constant_buffer_slot : 0);

            TagHash cbTag(cbId);
            if (ensureCBufferPayload(cbId, cbTag)) {
                fPSCB = EnqueueCBuffer(cbId).future;
            }
            else {
                OutputDebugStringA("[Tech] ensureCBufferPayload failed\n");
            }
        }
        else {
            if (Tfx.PixelShader.SamplerFallback.size() != 0) {
                try {
                    auto cb = createCBufferFromRaw_(Tfx.PixelShader.SamplerFallback.data(), Tfx.PixelShader.SamplerFallback.size() * 0x10);
                    if (cb) {
                        // Reuse your existing vectors so your draw code stays unchanged
                        tech->CBuffers_fallback = cb;
                        tech->psCBSlots_fallback = 0;
                    }
                }
                catch (const std::exception& e) {
                    printf("[Tech] %08X fallback PS cbuffer failed: %s\n", id, e.what());
                }
            }
        }

        
            
        if (Tfx.PixelShader.SamplerFallback.size() != 0) {
            try {
                auto cb = createCBufferFromRaw_(Tfx.VertexShader.SamplerFallback.data(), Tfx.VertexShader.SamplerFallback.size() * 0x10);
                if (cb) {
                    // Reuse your existing vectors so your draw code stays unchanged
                    tech->CBuffers_fallback_VS = cb;
                    tech->vsCBSlots_fallback = 0;
                }
            }
            catch (const std::exception& e) {
                printf("[Tech] %08X fallback PS cbuffer failed: %s\n", id, e.what());
            }
        }
        

        // --------- Collect everything ----------
        if (fVS.valid()) if (auto vs = fVS.get()) tech->VS.push_back(vs);
        if (fPS.valid()) if (auto ps = fPS.get()) tech->PS.push_back(ps);

        // Textures
        tech->Textures.reserve(psTexF.size());
        tech->psTextureSlots.reserve(psTexF.size());
        for (auto& [slot, fut] : psTexF) {
            if (!fut.valid()) continue;
            if (auto texRes = fut.get()) {
                tech->Textures.push_back(texRes);
                tech->psTextureSlots.push_back(slot);
            }
        }
        tech->Textures3D.reserve(psTexF3D.size());
        tech->psTextureSlots3D.reserve(psTexF3D.size());
        for (auto& [slot, fut] : psTexF3D) {
            if (!fut.valid()) continue;
            if (auto texRes = fut.get()) {
                tech->Textures3D.push_back(texRes);
                tech->psTextureSlots3D.push_back(slot);
            }
        }

        // NEW: Samplers
        // ---------- COLLECT ----------
        tech->Samplers.reserve(psSampF.size());
        tech->psSamplerSlots.reserve(psSampF.size());
        for (auto& [slot, fut] : psSampF) {
            if (auto sres = fut.get()) {
                tech->Samplers.push_back(sres);
                tech->psSamplerSlots.push_back(slot);
            }
        }

        if (fPSCB.valid()) {
            if (auto cbres = fPSCB.get()) {
                tech->CBuffers.push_back(cbres);
                tech->psCBSlots.push_back(psCBSlot);
            }
        } 
        tech->vertexdata = Tfx.VertexShader;
        tech->pixeldata = Tfx.PixelShader;
        tech->StateSelection = Tfx.StateSelection;
        tech->usedScopes = Tfx.UsedScopes;
        return tech;
        });
}



