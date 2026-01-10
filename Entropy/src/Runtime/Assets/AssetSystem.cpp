#include "AssetSystem.h"
#include <stdexcept>
#include <cassert>
#include <optional> 
using Microsoft::WRL::ComPtr;

#include <string>
#include <sstream>
#include <iomanip>


static inline UINT FullMipCount(UINT w, UINT h)
{
    if (w == 0 || h == 0) return 1;
    UINT m = 1;
    UINT s = (w > h ? w : h);
    while (s > 1) { s >>= 1; ++m; }
    return m;
}

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




AssetSystem::AssetSystem(ID3D11Device* device, ID3D11DeviceContext* context,
    ThreadPool& pool,
    MainThreadQueue& mainThread,
    RuntimeAssetRegistry* registry)
    : device_(device), context_(context),pool_(pool), mainThread_(mainThread), R_(registry)
{
    assert(device_ && "AssetSystem requires a valid ID3D11Device*");
}




AssetHandle<ID3D11Buffer> AssetSystem::EnqueueBuffer(uint32_t id)
{
    
    auto fut = bufferCache_.GetOrLoad(id, [=] {
        BufferPayload payload = R_->GetBuffer(id);
        return createBuffer_(payload);
        });

    AssetHandle<ID3D11Buffer> h;
    h.future = fut;              
    return h;
}
std::shared_ptr<ID3D11Buffer> AssetSystem::createBuffer_(const BufferPayload& p)
{
    D3D11_BUFFER_DESC desc = p.desc;
    const bool hasInit = !p.data.empty();
    if (hasInit && (desc.Usage == D3D11_USAGE_DEFAULT) && (desc.CPUAccessFlags == 0)) {
        desc.Usage = D3D11_USAGE_IMMUTABLE;
    }

    D3D11_SUBRESOURCE_DATA srd{};
    srd.pSysMem = hasInit ? (const void*)p.data.data() : nullptr;

    Microsoft::WRL::ComPtr<ID3D11Buffer> buf;
    HRESULT hr = device_->CreateBuffer(&desc, hasInit ? &srd : nullptr, &buf);
    if (FAILED(hr)) throw std::runtime_error("CreateBuffer failed");

    return std::shared_ptr<ID3D11Buffer>(buf.Detach(),
        [](ID3D11Buffer* b) { if (b) b->Release(); });
}




std::shared_ptr<EntropyAssets::VertexShader>
AssetSystem::createVertexShader_(uint32_t id, const ShaderPayload& p, uint32_t tech_id)
{
    ComPtr<ID3D11VertexShader> vs;
    HRESULT hr = device_->CreateVertexShader(
        p.bytecode.data(), p.bytecode.size(), nullptr, &vs);
    if (FAILED(hr)) {
        printf("CreateVS hr=0x%08X size=%zu magic=0x%08X\n", (unsigned)hr, p.bytecode.size(),
            p.bytecode.size() >= 4 ? *(const uint32_t*)p.bytecode.data() : 0u);
        throw std::runtime_error("CreateVertexShader failed");
    }
    SetDebugNameFmt(vs.Get(), "TECH_%08X", tech_id);
    
     

    auto out = std::make_shared<EntropyAssets::VertexShader>();
    out->vs = vs;

    return out;
}

std::shared_ptr<EntropyAssets::PixelShader>
AssetSystem::createPixelShader_(const ShaderPayload& p, uint32_t tech_id)
{
    ComPtr<ID3D11PixelShader> ps;
    HRESULT hr = device_->CreatePixelShader(
        p.bytecode.data(), p.bytecode.size(), nullptr, &ps);
    if (FAILED(hr)) {
        throw std::runtime_error("CreatePixelShader failed");
    }
    SetDebugNameFmt(ps.Get(), "TECH_%08X", tech_id);
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

AssetHandle<EntropyAssets::VertexShader> AssetSystem::EnqueueVertexShader(uint32_t id, uint32_t tech_id)
{
	
    auto fut = pool_.Submit([=] {
        return vsCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            return createVertexShader_(id, payload,tech_id);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::VertexShader> h; h.future = fut; return h;
}

AssetHandle<EntropyAssets::PixelShader> AssetSystem::EnqueuePixelShader(uint32_t id, uint32_t tech_id)
{
    
    auto fut = pool_.Submit([=] {
        return psCache_.GetOrLoad(id, [&] {
            ShaderPayload payload = R_->GetShader(id);
            return createPixelShader_(payload, tech_id);
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
            
            BufferPayload payload = R_->GetBuffer(id);
			payload.id = id;
            return createBufferSRV_(payload, meta);
            }).get();
        }).share();

    AssetHandle<EntropyAssets::BufferSRVRes> h; h.future = fut; return h;
}






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

    const UINT arraySize = std::max<UINT>(1, p.desc.ArraySize);
    const UINT wantedMips = std::max<UINT>(1, p.desc.MipLevels);
    const size_t provided = p.subresources.size();

    const UINT fullMips = FullMipCount(p.desc.Width, p.desc.Height);
    const bool providedAllWanted = (provided == size_t(arraySize) * size_t(wantedMips));

    ComPtr<ID3D11Texture2D> tex;

    if (providedAllWanted) {
        
        HRESULT hr = device_->CreateTexture2D(&p.desc,
            p.subresources.empty() ? nullptr : p.subresources.data(),
            &tex);
        if (FAILED(hr)) throw std::runtime_error("CreateTexture2D failed (full chain provided)");
    }
    else {
        
        D3D11_TEXTURE2D_DESC d = p.desc;

        
        d.MipLevels = std::max<UINT>(wantedMips, fullMips);
        d.BindFlags |= D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
        d.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;

        HRESULT hr = device_->CreateTexture2D(&d, nullptr, &tex);
        if (FAILED(hr)) throw std::runtime_error("CreateTexture2D failed (autogen)");

        ID3D11DeviceContext* ctx = context_;
        const UINT mipLevels = d.MipLevels;

        
        if (provided > 0) {
            const size_t perSlice = (provided >= arraySize) ? (provided / arraySize) : 1;
            for (UINT slice = 0; slice < arraySize; ++slice) {
                const size_t idx = std::min<size_t>(slice * perSlice, provided - 1);
                const auto& s = p.subresources[idx]; 
                const UINT sub = D3D11CalcSubresource(0, slice, mipLevels);
                ctx->UpdateSubresource(tex.Get(), sub, nullptr, s.pSysMem, s.SysMemPitch, s.SysMemSlicePitch);
            }
        }

        
        D3D11_SHADER_RESOURCE_VIEW_DESC tmp{};
        tmp.Format = p.desc.Format; 
        if (d.ArraySize > 1) {
            tmp.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            tmp.Texture2DArray.MostDetailedMip = 0;
            tmp.Texture2DArray.MipLevels = -1;
            tmp.Texture2DArray.FirstArraySlice = 0;
            tmp.Texture2DArray.ArraySize = d.ArraySize;
        }
        else {
            tmp.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            tmp.Texture2D.MostDetailedMip = 0;
            tmp.Texture2D.MipLevels = -1;
        }
        ComPtr<ID3D11ShaderResourceView> tmpSrv;
        device_->CreateShaderResourceView(tex.Get(), &tmp, &tmpSrv);
        ctx->GenerateMips(tmpSrv.Get());
    }

    
    auto ToTypedForSRV = [](DXGI_FORMAT f)->DXGI_FORMAT {
        switch (f) {
        case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM; 
        case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM; 
        case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;      
        case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;     
        case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;     
        case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;     
        default:                            return f;
        }
        };

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = ToTypedForSRV(p.desc.Format);

    D3D11_TEXTURE2D_DESC finalDesc{};
    tex->GetDesc(&finalDesc);

    
    const bool isCube = (finalDesc.MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) != 0;

    if (isCube)
    {
        const UINT totalFaces = finalDesc.ArraySize;
        const UINT numCubes = (totalFaces > 0) ? (totalFaces / 6) : 0;

        if (numCubes <= 1)
        {
            
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
            sd.TextureCube.MostDetailedMip = 0;
            sd.TextureCube.MipLevels = -1;
        }
        else
        {
            
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
            sd.TextureCubeArray.MostDetailedMip = 0;
            sd.TextureCubeArray.MipLevels = -1;
            sd.TextureCubeArray.First2DArrayFace = 0;
            sd.TextureCubeArray.NumCubes = numCubes;
        }
    }
    else
    {
        
        if (finalDesc.ArraySize > 1) {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
            sd.Texture2DArray.MostDetailedMip = 0;
            sd.Texture2DArray.MipLevels = -1;
            sd.Texture2DArray.FirstArraySlice = 0;
            sd.Texture2DArray.ArraySize = finalDesc.ArraySize;
        }
        else {
            sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
            sd.Texture2D.MostDetailedMip = 0;
            sd.Texture2D.MipLevels = -1;
        }
    }

    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device_->CreateShaderResourceView(tex.Get(), &sd, &srv);
    if (FAILED(hr)) {
        throw std::runtime_error("CreateShaderResourceView failed");
    }

    
    
    

    auto out = std::make_shared<EntropyAssets::Texture2DRes>();
    out->width = p.desc.Width;
    out->height = p.desc.Height;
    out->arraySize = p.desc.ArraySize;

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
    bd.ByteWidth = (byteSize + 15u) & ~15u; 

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


    std::shared_ptr<ID3D11Buffer> existing;
    try {
        existing = bufferCache_.GetOrLoad(p.id, [&] { return createBuffer_(p); }).get(); 
    }
    catch (...) {
        
    }

    ComPtr<ID3D11Buffer> bufCom;
    if (existing) {
        
        ID3D11Buffer* raw = existing.get();
        if (raw) { raw->AddRef(); bufCom.Attach(raw); } 
    }
    else {
     
        D3D11_BUFFER_DESC bd = p.desc;
        const bool hasInit = !p.data.empty();
        if (hasInit && bd.Usage == D3D11_USAGE_DEFAULT && bd.CPUAccessFlags == 0)
            bd.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA srd{};
        srd.pSysMem = hasInit ? (const void*)p.data.data() : nullptr;

        HRESULT hr = device_->CreateBuffer(&bd, hasInit ? &srd : nullptr, &bufCom);
        if (FAILED(hr)) throw std::runtime_error("CreateBuffer (SRV) failed");
    }

   
    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
    sd.Format = meta.typedFormat;            
    sd.Buffer.FirstElement = 0;
    sd.Buffer.NumElements = std::max<UINT>(1, p.desc.ByteWidth / std::max<UINT>(1, meta.bytesPerElement));

    ComPtr<ID3D11ShaderResourceView> srv;
    HRESULT hr = device_->CreateShaderResourceView(bufCom.Get(), &sd, &srv);
    if (FAILED(hr)) throw std::runtime_error("CreateShaderResourceView (buffer) failed");

    auto out = std::make_shared<EntropyAssets::BufferSRVRes>();
    out->buffer = existing.get()
        ? existing.get()
        : std::shared_ptr<ID3D11Buffer>(bufCom.Detach(), [](ID3D11Buffer* b) { if (b) b->Release(); }).get();

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
    bd.ByteWidth = byteWidth;                   
    bd.Usage = D3D11_USAGE_DYNAMIC;       
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



std::shared_future<std::shared_ptr<EntropyAssets::Technique>>
AssetSystem::EnqueueTechnique(TagHash techniqueId)
{
    const uint32_t id = techniqueId.hash;
    

    static AssetCache<EntropyAssets::Technique> techCache;

    return techCache.GetOrLoad(id, [=] {
        auto tech = std::make_shared<EntropyAssets::Technique>();
        tech->id = id;

        if (!techniqueId.data || techniqueId.size == 0) {
            printf("[Tech] %08X empty tag\n", id);
            return tech;
        }

        STechnique Tfx = bin::parse<STechnique>(techniqueId.data, techniqueId.size, bin::Endian::Little);

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

              
                TagHash sTag(sampId);
                std::optional<D3D11_SAMPLER_DESC> descOpt;
                if (sTag.data && sTag.size) {
                  
                    descOpt = BuildSamplerDescFromTag(sTag);
                }

         
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
                    
                }
                else {
                
                    meta.byteSize = 16u;
                }

                R_->RegisterCBuffer(cbId, std::move(meta));
                return true;
            };
        std::shared_future<std::shared_ptr<EntropyAssets::VertexShader>> fVS;
        std::shared_future<std::shared_ptr<EntropyAssets::PixelShader>>  fPS;
        if (ensureShaderPayload(vsId)) fVS = EnqueueVertexShader(vsId,techniqueId.hash).future;
        if (ensureShaderPayload(psId)) fPS = EnqueuePixelShader(psId, techniqueId.hash).future;

        
        using TexFuture = std::shared_future<std::shared_ptr<EntropyAssets::Texture2DRes>>;
        using TexFuture3D = std::shared_future<std::shared_ptr<EntropyAssets::Texture3DRes>>;
        std::vector<std::pair<UINT, TexFuture>> psTexF;
        std::vector<std::pair<UINT, TexFuture3D>> psTexF3D;
        psTexF.reserve(Tfx.PixelShader.Textures.size());
        for (const auto& t : Tfx.PixelShader.Textures) {
            const UINT slot = t.TextureIndex;
            const uint32_t texId = t.Texture.tagHash32;
            TagHash texTag(texId);
            if (texTag.sub_type == 3) {
                
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
            else if (texTag.sub_type == 1) {
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
            else if (texTag.sub_type == 2) {
                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTextureCubePayloadFromTag(texTag)) {
                        
                        R_->RegisterTexture(texId, std::move(*payload));
                    }
                    else {
                        continue; 
                    }
                }

      
                psTexF.emplace_back(slot, EnqueueTexture(texId).future);
            }
            
            
            
        }

        std::vector<std::pair<UINT, TexFuture>> vsTexF;
        std::vector<std::pair<UINT, TexFuture3D>> vsTexF3D;
        psTexF.reserve(Tfx.VertexShader.Textures.size());
        for (const auto& t : Tfx.VertexShader.Textures) {
            const UINT slot = t.TextureIndex;
            const uint32_t texId = t.Texture.tagHash32;
            TagHash texTag(texId);
            if (texTag.sub_type == 3) {

                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTexture3DPayloadFromTag(texTag)) {
                        R_->Register3DTexture(texId, std::move(*payload));
                    }
                    else {
                        continue;
                    }
                }
                vsTexF3D.emplace_back(slot, Enqueue3DTexture(texId).future);
            }
            else if (texTag.sub_type == 1) {
                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTexturePayloadFromTag(texTag)) {
                        R_->RegisterTexture(texId, std::move(*payload));
                    }
                    else {
                        continue;
                    }
                }
                vsTexF.emplace_back(slot, EnqueueTexture(texId).future);
            }
            else if (texTag.sub_type == 2) {
                if (!R_->HasTexture(texId)) {
                    if (auto payload = BuildTextureCubePayloadFromTag(texTag)) {

                        R_->RegisterTexture(texId, std::move(*payload));
                    }
                    else {
                        continue;
                    }
                }


                vsTexF.emplace_back(slot, EnqueueTexture(texId).future);
            }



        }

        using SampFuture = std::shared_future<std::shared_ptr<EntropyAssets::SamplerRes>>;
        std::vector<std::pair<UINT, SampFuture>> psSampF;
        psSampF.reserve(Tfx.PixelShader.Samplers.size());

        for (size_t i = 0; i < Tfx.PixelShader.Samplers.size(); ++i)
        {
            const auto& s = Tfx.PixelShader.Samplers[i];
            const uint32_t id = s.sampler.reference;                     

         
            const UINT slot = i;

           
            if (!ensureSamplerPayload(id)) continue;

            psSampF.emplace_back(slot, EnqueueSampler(id).future);
        }


        std::vector<std::pair<UINT, SampFuture>> vsSampF;
        vsSampF.reserve(Tfx.VertexShader.Samplers.size());

        for (size_t i = 0; i < Tfx.VertexShader.Samplers.size(); ++i)
        {
            const auto& s = Tfx.VertexShader.Samplers[i];
            const uint32_t id = s.sampler.reference;


            const UINT slot = i;


            if (!ensureSamplerPayload(id)) continue;

            vsSampF.emplace_back(slot, EnqueueSampler(id).future);
        }
        
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
                    
                    tech->CBuffers_fallback_VS = cb;
                    tech->vsCBSlots_fallback = 0;
                }
            }
            catch (const std::exception& e) {
                printf("[Tech] %08X fallback PS cbuffer failed: %s\n", id, e.what());
            }
        }
        
        std::shared_future<std::shared_ptr<EntropyAssets::CBufferRes>> fVSCB;
        UINT vsCBSlot = 0;
        const uint32_t cbId_vs = Tfx.VertexShader.contstant_buffer.reference;
        if (cbId_vs != 0u && cbId_vs != 0xFFFFFFFFu) {
            vsCBSlot = static_cast<UINT>(
                Tfx.VertexShader.constant_buffer_slot >= 0 ? Tfx.VertexShader.constant_buffer_slot : 0);

            TagHash cbTag(cbId_vs);
            if (ensureCBufferPayload(cbId_vs, cbTag)) {
                fVSCB = EnqueueCBuffer(cbId_vs).future;
            }
            else {
                OutputDebugStringA("[Tech] ensureCBufferPayload failed\n");
            }
        }

        
        if (fVS.valid()) if (auto vs = fVS.get()) tech->VS.push_back(vs);
        if (fPS.valid()) if (auto ps = fPS.get()) tech->PS.push_back(ps);

        
        tech->Textures_VS.reserve(vsTexF.size());
        tech->vsTextureSlots.reserve(vsTexF.size());
        for (auto& [slot, fut] : vsTexF) {
            if (!fut.valid()) continue;
            if (auto texRes = fut.get()) {
                tech->Textures_VS.push_back(texRes);
                tech->vsTextureSlots.push_back(slot);
            }
        }
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

        tech->Textures3D_VS.reserve(vsTexF3D.size());
        tech->vsTextureSlots3D.reserve(vsTexF3D.size());
        for (auto& [slot, fut] : vsTexF3D) {
            if (!fut.valid()) continue;
            if (auto texRes = fut.get()) {
                tech->Textures3D_VS.push_back(texRes);
                tech->vsTextureSlots3D.push_back(slot);
            }
        }

        tech->Samplers.reserve(psSampF.size());
        tech->psSamplerSlots.reserve(psSampF.size());
        for (auto& [slot, fut] : psSampF) {
            if (auto sres = fut.get()) {
                tech->Samplers.push_back(sres);
                tech->psSamplerSlots.push_back(slot);
            }
        }

        tech->Samplers_VS.reserve(vsSampF.size());
        tech->vsSamplerSlots.reserve(vsSampF.size());
        for (auto& [slot, fut] : vsSampF) {
            if (auto sres = fut.get()) {
                tech->Samplers_VS.push_back(sres);
                tech->vsSamplerSlots.push_back(slot);
            }
        }

        if (fPSCB.valid()) {
            if (auto cbres = fPSCB.get()) {
                tech->CBuffers.push_back(cbres);
                tech->psCBSlots.push_back(psCBSlot);
            }
        } 
        if (fVSCB.valid()) {
            if (auto cbres = fVSCB.get()) {
                tech->CBuffers_VS.push_back(cbres);
                tech->vsCBSlots.push_back(vsCBSlot);
            }
        }
        tech->vertexdata = Tfx.VertexShader;
        tech->pixeldata = Tfx.PixelShader;
        tech->StateSelection = Tfx.StateSelection;
        tech->usedScopes = Tfx.UsedScopes;
        return tech;
        });
}



