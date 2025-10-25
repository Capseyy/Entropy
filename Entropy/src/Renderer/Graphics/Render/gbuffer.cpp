// GBuffer.cpp
#include "GBuffer.h"
#include <stdexcept>
#include <algorithm>
#undef min
#undef max

using namespace DirectX;
using namespace DirectX::PackedVector;


static void ThrowIfFailed(HRESULT hr, const char* msg) {
    if (FAILED(hr)) throw std::runtime_error(msg);
}

static inline void SetDbgName(ID3D11DeviceChild* o, const char* n) {
#if _DEBUG
    if (o && n) o->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(n), n);
#endif
}

bool CreateColorRT(ID3D11Device* dev, UINT w, UINT h,
    DXGI_FORMAT texTypeless, DXGI_FORMAT rtvFmt, DXGI_FORMAT srvFmt,
    const char* debugName, RenderTarget& out)
{
    if (!dev) return false;
    out.Reset();

    D3D11_TEXTURE2D_DESC td{};
    td.Width = std::max<UINT>(w, 1);
    td.Height = std::max<UINT>(h, 1);
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = texTypeless;
    td.SampleDesc = { 1,0 };
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

    ThrowIfFailed(dev->CreateTexture2D(&td, nullptr, out.tex.GetAddressOf()), "CreateTexture2D");
    SetDbgName(out.tex.Get(), debugName);

    D3D11_RENDER_TARGET_VIEW_DESC rvd{};
    rvd.Format = rtvFmt;
    rvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
    ThrowIfFailed(dev->CreateRenderTargetView(out.tex.Get(), &rvd, out.rtv.GetAddressOf()), "CreateRTV");

    D3D11_SHADER_RESOURCE_VIEW_DESC svd{};
    svd.Format = srvFmt;
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Texture2D.MostDetailedMip = 0;
    svd.Texture2D.MipLevels = 1;
    ThrowIfFailed(dev->CreateShaderResourceView(out.tex.Get(), &svd, out.srv.GetAddressOf()), "CreateSRV");

    out.baseFormat = texTypeless;
    out.rtvFormat = rtvFmt;
    out.srvFormat = srvFmt;
    out.name = debugName ? debugName : "";
    return true;
}

bool CreateStaging(ID3D11Device* dev, UINT w, UINT h,
    DXGI_FORMAT fmt, const char* name, CpuStagingBuffer& out)
{
    if (!dev) return false;
    out = CpuStagingBuffer{};
    D3D11_TEXTURE2D_DESC td{};
    td.Width = std::max<UINT>(w, 1);
    td.Height = std::max<UINT>(h, 1);
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc = { 1,0 };
    td.Usage = D3D11_USAGE_STAGING;
    td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

    ThrowIfFailed(dev->CreateTexture2D(&td, nullptr, out.tex.GetAddressOf()), "Create staging");
    out.fmt = fmt;
    out.name = name ? name : "";
    SetDebugName(out.tex.Get(), out.name.c_str());
    return true;
}

bool GBuffer::ReadLightDiffusePixel(ID3D11DeviceContext* ctx, UINT x, UINT y,
    XMFLOAT3& outRGB) const
{
    if (!ctx || !light_diffuse.tex) return false;

    x = std::min<UINT>(x, std::max(1u, w) - 1);
    y = std::min<UINT>(y, std::max(1u, h) - 1);

    if (!lightDiffuseReadback1x1) {
        D3D11_TEXTURE2D_DESC td = {};
        td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R11G11B10_FLOAT;
        td.SampleDesc = { 1,0 };
        td.Usage = D3D11_USAGE_STAGING;
        td.CPUAccessFlags = D3D11_CPU_ACCESS_READ;

        Microsoft::WRL::ComPtr<ID3D11Device> dev;
        ctx->GetDevice(dev.GetAddressOf());
        if (!dev) return false;

        if (FAILED(dev->CreateTexture2D(&td, nullptr, lightDiffuseReadback1x1.GetAddressOf())))
            return false;
        SetDbgName(lightDiffuseReadback1x1.Get(), "LightDiffuse_Readback_1x1");
    }

    D3D11_BOX box{ x, y, 0, x + 1, y + 1, 1 };
    ctx->CopySubresourceRegion(lightDiffuseReadback1x1.Get(), 0, 0, 0, 0,
        light_diffuse.tex.Get(), 0, &box);

    D3D11_MAPPED_SUBRESOURCE ms{};
    if (FAILED(ctx->Map(lightDiffuseReadback1x1.Get(), 0, D3D11_MAP_READ, 0, &ms)))
        return false;

    const uint32_t packed = *reinterpret_cast<const uint32_t*>(ms.pData);
    ctx->Unmap(lightDiffuseReadback1x1.Get(), 0);

    XMFLOAT3PK pk; *reinterpret_cast<uint32_t*>(&pk) = packed;
    XMVECTOR v = XMLoadFloat3PK(&pk); // linear RGB from R11G11B10_FLOAT
    XMStoreFloat3(&outRGB, v);
    return true;
}

bool CreateDepthState(ID3D11Device* dev, UINT w, UINT h,
    const char* name, DepthState& out)
{
    if (!dev) return false;
    out.Reset();

    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = std::max<UINT>(w, 1);
    dd.Height = std::max<UINT>(h, 1);
    dd.MipLevels = 1; dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_R32_TYPELESS;
    dd.SampleDesc = { 1,0 };
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

    ThrowIfFailed(dev->CreateTexture2D(&dd, nullptr, out.tex.GetAddressOf()), "Create depth tex");
    SetDbgName(out.tex.Get(), (std::string(name) + " (Texture)").c_str());

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
    dsvd.Format = DXGI_FORMAT_D32_FLOAT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    dsvd.Texture2D.MipSlice = 0;
    ThrowIfFailed(dev->CreateDepthStencilView(out.tex.Get(), &dsvd, out.dsv.GetAddressOf()), "Create DSV");

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = DXGI_FORMAT_R32_FLOAT;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MostDetailedMip = 0;
    sd.Texture2D.MipLevels = 1;
    ThrowIfFailed(dev->CreateShaderResourceView(out.tex.Get(), &sd, out.srv.GetAddressOf()), "Create depth SRV");

    // optional copy SRV
    D3D11_TEXTURE2D_DESC cd = dd; cd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    ThrowIfFailed(dev->CreateTexture2D(&cd, nullptr, out.texCopy.GetAddressOf()), "Create depth copy");
    ThrowIfFailed(dev->CreateShaderResourceView(out.texCopy.Get(), &sd, out.texCopySRV.GetAddressOf()), "Create copy SRV");

    D3D11_DEPTH_STENCIL_DESC wr{};
    wr.DepthEnable = TRUE; wr.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL; wr.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
    ThrowIfFailed(dev->CreateDepthStencilState(&wr, out.dsWrite.GetAddressOf()), "Create dsWrite");
    D3D11_DEPTH_STENCIL_DESC ro = wr; ro.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
    ThrowIfFailed(dev->CreateDepthStencilState(&ro, out.dsReadOnly.GetAddressOf()), "Create dsReadOnly");

    return true;
}

// ---------------------- GBuffer ----------------------

void GBuffer::Create(ID3D11Device* dev, UINT W, UINT H)
{
    if (!dev) throw std::runtime_error("GBuffer::Create: null device");
    if (W == 0 || H == 0) { W = 1; H = 1; }
    w = W; h = H;

    // Geometry
    if (!CreateColorRT(dev, W, H, DXGI_FORMAT_B8G8R8A8_TYPELESS,
        DXGI_FORMAT_B8G8R8A8_UNORM, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB,
        "RT0_Albedo", rt0)) throw std::runtime_error("rt0");

    if (!CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R10G10B10A2_UNORM, "RT1_NormalRough", rt1))
        throw std::runtime_error("rt1");
    if (!CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R10G10B10A2_UNORM, "RT1_Clone", rt1_read))
        throw std::runtime_error("rt1_clone");
    if (!CreateColorRTSimple(dev, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, "RT2_Material", rt2))
        throw std::runtime_error("rt2");
    if (!CreateColorRTSimple(dev, W, H, DXGI_FORMAT_B8G8R8A8_UNORM, "RT3_Extra", rt3))
        throw std::runtime_error("rt3");

    // Lighting
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R11G11B10_FLOAT, "Light_Diffuse", light_diffuse);
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R11G11B10_FLOAT, "Light_Specular", light_specular);
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R11G11B10_FLOAT, "Specular_IBL", light_ibl_specular);

    // Shading/output
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R11G11B10_FLOAT, "Shading", shading_result);
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R11G11B10_FLOAT, "Shading_Clone", shading_result_read);

    // Depth + staging
    if (!CreateDepthState(dev, W, H, "gbuffer_depth", depth)) throw std::runtime_error("depth");
    if (!CreateStaging(dev, W, H, DXGI_FORMAT_R32_TYPELESS, "Depth_Staging", depth_staging))
        throw std::runtime_error("depth_staging");

    // Misc
    CreateColorRTSimple(dev, W, H, DXGI_FORMAT_R8_UNORM, "SSAO_Intermediate", ssao_intermediate);
    CreateColorRTSimple(dev, W / 4, H / 4, DXGI_FORMAT_R16G16B16A16_FLOAT, "atmos_ss_far_lookup", atmos_ss_far_lookup);
    CreateColorRTSimple(dev, W / 4, H / 4, DXGI_FORMAT_R16G16B16A16_FLOAT, "atmos_ss_near_lookup", atmos_ss_near_lookup);
    CreateColorRTSimple(dev, 512, 512, DXGI_FORMAT_R16G16B16A16_FLOAT, "depth_angle_density_lookup", depth_angle_density_lookup);

    // Postprocess ping/pong (sRGB SRV)
    CreateColorRT(dev, W, H, DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        "postprocess_ping", postprocess_ping);
    CreateColorRT(dev, W, H, DXGI_FORMAT_R8G8B8A8_TYPELESS,
        DXGI_FORMAT_R8G8B8A8_UNORM, DXGI_FORMAT_R8G8B8A8_UNORM_SRGB,
        "postprocess_pong", postprocess_pong);
}

void GBuffer::CreateFromContext(ID3D11DeviceContext* ctx, UINT W, UINT H)
{
    if (!ctx) throw std::runtime_error("GBuffer::CreateFromContext: ctx is null");
    Microsoft::WRL::ComPtr<ID3D11Device> dev;
    ctx->GetDevice(dev.GetAddressOf());
    if (!dev) throw std::runtime_error("GBuffer::CreateFromContext: GetDevice returned null");
    Create(dev.Get(), W, H);
}

void GBuffer::Resize(ID3D11Device* dev, UINT W, UINT H) {
    // Proper reset instead of memset/memcpy
    *this = GBuffer{};
    Create(dev, W, H);
}

void GBuffer::BindGBufferForWriting(ID3D11DeviceContext* ctx) const {
    ID3D11RenderTargetView* rtvs[4] = { rt0.rtv.Get(), rt1.rtv.Get(), rt2.rtv.Get(), rt3.rtv.Get() };
    ctx->OMSetRenderTargets(4, rtvs, depth.dsv.Get());

    D3D11_VIEWPORT vp{};
    vp.TopLeftX = 0; vp.TopLeftY = 0;
    vp.Width = float(w);   // GBuffer size
    vp.Height = float(h);
    vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
    ctx->RSSetViewports(1, &vp);

    const float c0[4] = { 0,0,0,1 }, n0[4] = { 0.5f,0.5f,1,1 }, m0[4] = { 0,1,0,0 };
    ctx->ClearRenderTargetView(rt0.rtv.Get(), c0);
    ctx->ClearRenderTargetView(rt1.rtv.Get(), n0);
    ctx->ClearRenderTargetView(rt2.rtv.Get(), m0);
    ctx->ClearRenderTargetView(rt3.rtv.Get(), c0);
    ctx->ClearDepthStencilView(depth.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void GBuffer::UnbindMRTs(ID3D11DeviceContext* ctx) const {
    ID3D11RenderTargetView* nullRTV[4] = { nullptr,nullptr,nullptr,nullptr };
    ctx->OMSetRenderTargets(4, nullRTV, nullptr);
}
