
#include <wincodec.h>
#pragma comment(lib, "windowscodecs.lib")
#include "Graphics/Graphics.h"

static Microsoft::WRL::ComPtr<IWICImagingFactory> g_wicFactory;

static HRESULT EnsureWIC()
{
    if (g_wicFactory) return S_OK;
    return CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(g_wicFactory.GetAddressOf()));
}

static DXGI_FORMAT ToSRGB(DXGI_FORMAT fmt, bool srgb)
{
    if (!srgb) return fmt;
    switch (fmt)
    {
    case DXGI_FORMAT_R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    default: return fmt;
    }
}

static HRESULT CopyFrameToRGBA8(IWICBitmapSource* src, std::vector<uint8_t>& out, UINT& w, UINT& h)
{
    Microsoft::WRL::ComPtr<IWICFormatConverter> cvt;
    HRESULT hr = g_wicFactory->CreateFormatConverter(&cvt);
    if (FAILED(hr)) return hr;
    hr = cvt->Initialize(src, GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
        nullptr, 0.f, WICBitmapPaletteTypeCustom);
    if (FAILED(hr)) return hr;

    hr = cvt->GetSize(&w, &h);
    if (FAILED(hr)) return hr;

    const UINT stride = w * 4u;
    out.resize(size_t(stride) * size_t(h));
    return cvt->CopyPixels(nullptr, stride, (UINT)out.size(), out.data());
}

static HRESULT CreateSRVFromRGBA8(ID3D11Device* dev, const uint8_t* rgba, UINT w, UINT h, bool srgb,
    ID3D11ShaderResourceView** outSRV)
{
    D3D11_TEXTURE2D_DESC td{};
    td.Width = w; td.Height = h;
    td.MipLevels = 1; td.ArraySize = 1;
    td.Format = ToSRGB(DXGI_FORMAT_R8G8B8A8_UNORM, srgb);
    td.SampleDesc = { 1,0 };
    td.Usage = D3D11_USAGE_DEFAULT;
    td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rgba;
    init.SysMemPitch = w * 4;

    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    HRESULT hr = dev->CreateTexture2D(&td, &init, tex.GetAddressOf());
    if (FAILED(hr)) return hr;

    D3D11_SHADER_RESOURCE_VIEW_DESC svd{};
    svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    svd.Format = td.Format;
    svd.Texture2D.MostDetailedMip = 0;
    svd.Texture2D.MipLevels = 1;
    return dev->CreateShaderResourceView(tex.Get(), &svd, outSRV);
}

HRESULT LoadTextureFromFileWIC(ID3D11Device* dev, const std::filesystem::path& file, bool srgb,
    ID3D11ShaderResourceView** outSRV)
{
    *outSRV = nullptr;
    HRESULT hr = EnsureWIC(); if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
    hr = g_wicFactory->CreateDecoderFromFilename(file.c_str(), nullptr, GENERIC_READ,
        WICDecodeMetadataCacheOnLoad, &dec);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = dec->GetFrame(0, &frame);
    if (FAILED(hr)) return hr;

    std::vector<uint8_t> rgba;
    UINT w = 0, h = 0;
    hr = CopyFrameToRGBA8(frame.Get(), rgba, w, h);
    if (FAILED(hr)) return hr;

    return CreateSRVFromRGBA8(dev, rgba.data(), w, h, srgb, outSRV);
}

HRESULT LoadTextureFromMemoryWIC(ID3D11Device* dev, const void* bytes, size_t size, bool srgb,
    ID3D11ShaderResourceView** outSRV)
{
    *outSRV = nullptr;
    HRESULT hr = EnsureWIC(); if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICStream> stream;
    hr = g_wicFactory->CreateStream(&stream);
    if (FAILED(hr)) return hr;
    hr = stream->InitializeFromMemory(
        reinterpret_cast<WICInProcPointer>(const_cast<void*>(bytes)),
        static_cast<DWORD>(size));
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapDecoder> dec;
    hr = g_wicFactory->CreateDecoderFromStream(stream.Get(), nullptr, WICDecodeMetadataCacheOnLoad, &dec);
    if (FAILED(hr)) return hr;

    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    hr = dec->GetFrame(0, &frame);
    if (FAILED(hr)) return hr;

    std::vector<uint8_t> rgba;
    UINT w = 0, h = 0;
    hr = CopyFrameToRGBA8(frame.Get(), rgba, w, h);
    if (FAILED(hr)) return hr;

    return CreateSRVFromRGBA8(dev, rgba.data(), w, h, srgb, outSRV);
}


bool Graphics::LoadGlobalTextureOptional(const std::string& key,
    const std::filesystem::path& onDiskPath,
    int resourceId,
    bool forceSRGB)
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;

    
    if (!onDiskPath.empty() && std::filesystem::exists(onDiskPath))
    {
        if (SUCCEEDED(LoadTextureFromFileWIC(pDevice.Get(), onDiskPath, forceSRGB, srv.GetAddressOf())))
        {
            this->global_textures[key] = srv;
            return true;
        }
    }

    HMODULE mod = GetModuleHandleW(nullptr);
    HRSRC res = FindResourceW(mod, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
    if (res)
    {
        HGLOBAL h = LoadResource(mod, res);
        if (h)
        {
            DWORD sz = SizeofResource(mod, res);
            void* ptr = LockResource(h);
            if (ptr && sz)
            {
                if (LoadTextureFromMemoryWIC(pDevice.Get(), ptr, sz, forceSRGB, srv.GetAddressOf()))
                {
                    this->global_textures[key] = srv;
                    return true;
                }
            }
        }
    }

    
    if (white1x1SRV) {
        this->global_textures[key] = white1x1SRV;
        return false;
    }
    return false;
}