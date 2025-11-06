#include "TextureLoader.h"
#include <wincodec.h>                 // WIC
#pragma comment(lib, "windowscodecs.lib")
#include "TigerEngine/Technique/texture.h"

using Microsoft::WRL::ComPtr;

static ComPtr<IWICImagingFactory> g_wicFactory;

HRESULT EnsureWIC() {
    if (g_wicFactory) return S_OK;
    HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    if (hr == RPC_E_CHANGED_MODE) hr = S_OK; // already initialized in different mode
    if (FAILED(hr)) return hr;

    return CoCreateInstance(
        CLSID_WICImagingFactory2, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(g_wicFactory.GetAddressOf()));
}

// …helpers: format convert/copy to RGBA8 omitted for brevity …

// Minimal frame?RGBA8 copier (you can drop your fuller version in here)
static HRESULT CopyFrameToRGBA8(IWICBitmapFrameDecode* frame,
    std::vector<uint8_t>& rgba,
    UINT& w, UINT& h)
{
    GUID pf = {};
    HRESULT hr = frame->GetPixelFormat(&pf);
    if (FAILED(hr)) return hr;

    hr = frame->GetSize(&w, &h);
    if (FAILED(hr) || w == 0 || h == 0) return E_FAIL;

    rgba.resize(size_t(w) * h * 4);

    ComPtr<IWICFormatConverter> conv;
    hr = g_wicFactory->CreateFormatConverter(&conv);
    if (FAILED(hr)) return hr;

    hr = conv->Initialize(frame, GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone, nullptr, 0.0,
        WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return hr;

    return conv->CopyPixels(nullptr, w * 4, (UINT)rgba.size(), rgba.data());
}

HRESULT LoadTextureFromMemoryWIC(ID3D11Device* dev, const void* bytes, size_t size,
    bool srgb, ID3D11ShaderResourceView** outSRV)
{
    if (!dev || !bytes || !size || !outSRV) return E_INVALIDARG;
    *outSRV = nullptr;

    HRESULT hr = EnsureWIC(); if (FAILED(hr)) return hr;

    ComPtr<IWICStream> stream;
    hr = g_wicFactory->CreateStream(&stream);              if (FAILED(hr)) return hr;
    hr = stream->InitializeFromMemory(
        reinterpret_cast<WICInProcPointer>(const_cast<void*>(bytes)),
        static_cast<DWORD>(size));                         if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapDecoder> dec;
    hr = g_wicFactory->CreateDecoderFromStream(
        stream.Get(), nullptr, WICDecodeMetadataCacheOnDemand, &dec); if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = dec->GetFrame(0, &frame);                         if (FAILED(hr)) return hr;

    std::vector<uint8_t> rgba; UINT w = 0, h = 0;
    hr = CopyFrameToRGBA8(frame.Get(), rgba, w, h);        if (FAILED(hr)) return hr;

    return CreateSRVFromRGBA8(dev, rgba.data(), w, h, srgb, outSRV);
}

HRESULT LoadTextureFromFileWIC(ID3D11Device* dev, ID3D11DeviceContext*,
    const wchar_t* path, bool srgb,
    ID3D11ShaderResourceView** outSRV)
{
    if (!dev || !path || !outSRV) return E_INVALIDARG;
    *outSRV = nullptr;

    HRESULT hr = EnsureWIC(); if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapDecoder> dec;
    hr = g_wicFactory->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &dec); if (FAILED(hr)) return hr;

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = dec->GetFrame(0, &frame); if (FAILED(hr)) return hr;

    std::vector<uint8_t> rgba; UINT w = 0, h = 0;
    hr = CopyFrameToRGBA8(frame.Get(), rgba, w, h); if (FAILED(hr)) return hr;

    return CreateSRVFromRGBA8(dev, rgba.data(), w, h, srgb, outSRV);
}

HRESULT CreateSRVFromRGBA8(ID3D11Device* dev, const uint8_t* rgba, UINT w, UINT h,
    bool srgb, ID3D11ShaderResourceView** outSRV)
{
    if (!dev || !rgba || !w || !h || !outSRV) return E_INVALIDARG;
    *outSRV = nullptr;

    DXGI_FORMAT fmt = srgb ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB
        : DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D11_TEXTURE2D_DESC td = {};
    td.Width = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = fmt;
    td.SampleDesc = { 1,0 };
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{ rgba, w * 4, 0 };

    ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = dev->CreateTexture2D(&td, &init, tex.GetAddressOf());
    if (FAILED(hr)) return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC sd = {};
    sd.Format = fmt;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    sd.Texture2D.MipLevels = 1;

    return dev->CreateShaderResourceView(tex.Get(), &sd, outSRV);
}
