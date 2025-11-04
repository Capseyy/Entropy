// GBufferRT.h
#pragma once
#include <wrl.h>
#include <d3d11.h>
#include <string>
#include <stdexcept>
#include <wrl/client.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <atomic>
#include "d3dcompiler.h"

using Microsoft::WRL::ComPtr;


inline HRESULT SetDebugName(ID3D11DeviceChild* obj, const char* name) {
#if defined(_DEBUG)
    if (!obj || !name) return S_OK;
    return obj->SetPrivateData(WKPDID_D3DDebugObjectName, UINT(strlen(name)), name);
#else
    (void)obj; (void)name; return S_OK;
#endif
}

static HRESULT CompileVSFromMemory(
    ID3D11Device* device,
    const char* source,
    const char* entryPoint,
    const char* target,
    ID3D11VertexShader** outVS,
    ID3DBlob** outBytecode)   // keep bytecode so you can make the input layout
{
    UINT flags = 0;

    flags = D3DCOMPILE_OPTIMIZATION_LEVEL3;

    Microsoft::WRL::ComPtr<ID3DBlob> bytecode, errors;
    HRESULT hr = D3DCompile(
        source, strlen(source),
        /*sourceName*/ "SkinnedHackVS.hlsl",
        /*defines*/ nullptr,
        /*include*/ nullptr,
        entryPoint, target, flags, 0,
        bytecode.GetAddressOf(), errors.GetAddressOf());

    if (FAILED(hr)) {
        if (errors) OutputDebugStringA((const char*)errors->GetBufferPointer());
        return hr;
    }

    hr = device->CreateVertexShader(
        bytecode->GetBufferPointer(),
        bytecode->GetBufferSize(),
        nullptr,
        outVS);
    if (FAILED(hr)) return hr;

    if (outBytecode) {
        *outBytecode = bytecode.Detach();
    }
    return S_OK;
}


struct RenderTarget {
    ComPtr<ID3D11Texture2D>          tex;
    ComPtr<ID3D11RenderTargetView>   rtv;
    ComPtr<ID3D11ShaderResourceView> srv;
    DXGI_FORMAT                       format = DXGI_FORMAT_UNKNOWN;
    std::string                       name;

    static RenderTarget Create(ID3D11Device* dev, UINT w, UINT h, DXGI_FORMAT fmt, const char* debugName) {
        RenderTarget rt{};
        rt.format = fmt;
        rt.name = debugName ? debugName : "";

        if (w == 0 || h == 0) { w = 1; h = 1; }

        D3D11_TEXTURE2D_DESC td{};
        td.Width = w; td.Height = h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = fmt;
        td.SampleDesc = { 1, 0 };
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = dev->CreateTexture2D(&td, nullptr, rt.tex.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateTexture2D failed for RT");

        hr = dev->CreateRenderTargetView(rt.tex.Get(), nullptr, rt.rtv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateRenderTargetView failed");

        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = fmt;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        hr = dev->CreateShaderResourceView(rt.tex.Get(), &sd, rt.srv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateShaderResourceView failed");

        if (rt.tex) rt.tex->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)rt.name.size(), rt.name.data());
        return rt;
    }

    void Resize(ID3D11Device* dev, UINT w, UINT h) {
        *this = Create(dev, w, h, format, name.c_str());
    }

    void Clear(ID3D11DeviceContext* ctx, const float rgba[4]) const {
        ctx->ClearRenderTargetView(rtv.Get(), rgba);
    }
};

struct DepthState {
    ComPtr<ID3D11Texture2D>          tex;           // R32_TYPELESS
    ComPtr<ID3D11DepthStencilView>   dsv;           // D32_FLOAT
    ComPtr<ID3D11ShaderResourceView> srv;           // R32_FLOAT
    ComPtr<ID3D11DepthStencilState>  dsWrite;       // GREATER_EQUAL, writes ON
    ComPtr<ID3D11DepthStencilState>  dsReadOnly;    // GREATER_EQUAL, writes OFF

    // GPU-readable copy (typeless R32 -> SRV R32_FLOAT)
    ComPtr<ID3D11Texture2D>          texCopy;
    ComPtr<ID3D11ShaderResourceView> texCopySRV;

    static DepthState Create(ID3D11Device* dev, UINT w, UINT h, const char* debugName) {
        DepthState ds{};

        if (w == 0 || h == 0) { w = 4; h = 4; }

        // R32_TYPELESS so we can DSV(D32_FLOAT) + SRV(R32_FLOAT)
        D3D11_TEXTURE2D_DESC td{};
        td.Width = w; td.Height = h;
        td.MipLevels = 1; td.ArraySize = 1;
        td.Format = DXGI_FORMAT_R32_TYPELESS;
        td.SampleDesc = { 1,0 };
        td.Usage = D3D11_USAGE_DEFAULT;
        td.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;

        HRESULT hr = dev->CreateTexture2D(&td, nullptr, ds.tex.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Depth tex CreateTexture2D failed");

        // DSV view
        D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
        dsvd.Format = DXGI_FORMAT_D32_FLOAT;
        dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
        dsvd.Texture2D.MipSlice = 0;
        hr = dev->CreateDepthStencilView(ds.tex.Get(), &dsvd, ds.dsv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("CreateDepthStencilView failed");

        // SRV view
        D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
        sd.Format = DXGI_FORMAT_R32_FLOAT;
        sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
        sd.Texture2D.MostDetailedMip = 0;
        sd.Texture2D.MipLevels = 1;
        hr = dev->CreateShaderResourceView(ds.tex.Get(), &sd, ds.srv.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Depth SRV create failed");

        // Depth states (reversed-Z)
        CD3D11_DEPTH_STENCIL_DESC zsWrite(D3D11_DEFAULT);
        zsWrite.DepthEnable = TRUE;
        zsWrite.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
        zsWrite.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
        dev->CreateDepthStencilState(&zsWrite, ds.dsWrite.GetAddressOf());

        CD3D11_DEPTH_STENCIL_DESC zsRO = zsWrite;
        zsRO.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
        dev->CreateDepthStencilState(&zsRO, ds.dsReadOnly.GetAddressOf());

        // Copy texture + SRV (for sampling in lighting)
        td.BindFlags = D3D11_BIND_SHADER_RESOURCE;
        hr = dev->CreateTexture2D(&td, nullptr, ds.texCopy.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Depth copy tex CreateTexture2D failed");

        hr = dev->CreateShaderResourceView(ds.texCopy.Get(), &sd, ds.texCopySRV.GetAddressOf());
        if (FAILED(hr)) throw std::runtime_error("Depth copy SRV create failed");

        if (ds.tex) ds.tex->SetPrivateData(WKPDID_D3DDebugObjectName, (UINT)strlen(debugName), debugName);
        return ds;
    }

    void Resize(ID3D11Device* dev, UINT w, UINT h, const char* name) {
        *this = Create(dev, w, h, name);
    }

    void Clear(ID3D11DeviceContext* ctx, float depth = 0.0f, UINT8 stencil = 0) const {
        ctx->ClearDepthStencilView(dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
    }

    void CopyForSampling(ID3D11DeviceContext* ctx) const {
        ctx->CopyResource(texCopy.Get(), tex.Get());
    }
};

// GBufferRT.h (cont.)
struct GBufferRT {
    RenderTarget rt0;                  // B8G8R8A8_UNORM_SRGB  (albedo)
    RenderTarget rt1;                  // R10G10B10A2_UNORM    (normal.xy, roughness A, etc.)
    RenderTarget rt1_read;             // clone (for Load/Sample hazards)
    RenderTarget rt2;                  // B8G8R8A8_UNORM       (material params)
    RenderTarget rt3;                  // B8G8R8A8_UNORM       (optional)

    RenderTarget light_diffuse;        // R11G11B10_FLOAT
    RenderTarget light_specular;       // R11G11B10_FLOAT
    RenderTarget light_ibl_specular;   // R11G11B10_FLOAT

    RenderTarget shading_result;       // R11G11B10_FLOAT
    RenderTarget shading_result_read;  // R11G11B10_FLOAT

    RenderTarget ssao_intermediate;    // R8_UNORM
    RenderTarget atmos_ss_far_lookup;  // R16G16B16A16_FLOAT (quarter res)
    RenderTarget atmos_ss_near_lookup; // R16G16B16A16_FLOAT (quarter res)
    RenderTarget depth_angle_density_lookup; // 512x512 R16G16B16A16_FLOAT

    DepthState  depth;

    void Create(ID3D11Device* dev, UINT w, UINT h) {
        if (!w || !h) { w = 1; h = 1; }

        rt0 = RenderTarget::Create(dev, w, h, DXGI_FORMAT_B8G8R8A8_UNORM_SRGB, "RT0");
        rt1 = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R10G10B10A2_UNORM, "RT1");
        rt1_read = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R10G10B10A2_UNORM, "RT1_Clone");
        rt2 = RenderTarget::Create(dev, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, "RT2");
        rt3 = RenderTarget::Create(dev, w, h, DXGI_FORMAT_B8G8R8A8_UNORM, "RT3");

        light_diffuse = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R11G11B10_FLOAT, "Light_Diffuse");
        light_specular = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R11G11B10_FLOAT, "Light_Specular");
        light_ibl_specular = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R11G11B10_FLOAT, "Specular_IBL");

        shading_result = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R11G11B10_FLOAT, "Staging");
        shading_result_read = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R11G11B10_FLOAT, "Staging_Clone");

        ssao_intermediate = RenderTarget::Create(dev, w, h, DXGI_FORMAT_R8_UNORM, "SSAO_Intermediate");
        atmos_ss_far_lookup = RenderTarget::Create(dev, w / 4, h / 4, DXGI_FORMAT_R16G16B16A16_FLOAT, "atmos_ss_far_lookup");
        atmos_ss_near_lookup = RenderTarget::Create(dev, w / 4, h / 4, DXGI_FORMAT_R16G16B16A16_FLOAT, "atmos_ss_near_lookup");
        depth_angle_density_lookup = RenderTarget::Create(dev, 512, 512, DXGI_FORMAT_R16G16B16A16_FLOAT, "depth_angle_density_lookup");

        depth = DepthState::Create(dev, w, h, "gbuffer_depth");
    }

    void Resize(ID3D11Device* dev, UINT w, UINT h) {
        Create(dev, w, h);
    }

    // binds RT0/RT1/RT2 + depth, sets viewport (full)
    void BindGBufferForWriting(ID3D11DeviceContext* ctx) {
        ID3D11RenderTargetView* rts[3] = { rt0.rtv.Get(), rt1.rtv.Get(), rt2.rtv.Get() };
        ctx->OMSetRenderTargets(3, rts, depth.dsv.Get());
        D3D11_VIEWPORT vp{ 0,0,(float)Desc(rt0).Width,(float)Desc(rt0).Height,0,1 };
        ctx->RSSetViewports(1, &vp);
    }

    static D3D11_TEXTURE2D_DESC Desc(const RenderTarget& rt) {
        D3D11_TEXTURE2D_DESC d{}; rt.tex->GetDesc(&d); return d;
    }
};

