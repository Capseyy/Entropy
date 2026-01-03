#include "TigerEngine/Technique/texture.h"
#undef max
#undef min
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

inline std::pair<size_t, size_t>
calculate_pitch(DXGI_FORMAT fmt, size_t width, size_t height)
{
    switch (fmt)
    {
        
    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_TYPELESS:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
    {
        const size_t nbw = std::max<size_t>(1, (width + 3) / 4);
        const size_t nbh = std::max<size_t>(1, (height + 3) / 4);
        const size_t pitch = nbw * 8;
        return { pitch, pitch * nbh };
    }

    
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_TYPELESS:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_TYPELESS:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
    {
        const size_t nbw = std::max<size_t>(1, (width + 3) / 4);
        const size_t nbh = std::max<size_t>(1, (height + 3) / 4);
        const size_t pitch = nbw * 16;
        return { pitch, pitch * nbh };
    }

    default:
        break;
    }
}

ID3D11ShaderResourceView* TigerTexture::GetTexture()
{
    return textureView.Get();
}

#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <algorithm>
#include <cstdint>

using Microsoft::WRL::ComPtr;

struct TigerHeader
{
    uint32_t width;
    uint32_t height;
    uint32_t mipCount;      
    uint32_t dxgiFormat;    
    
};


inline bool IsBCFormat(DXGI_FORMAT f, UINT& blockBytes)
{
    switch (f)
    {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM:
        blockBytes = 8;  return true;

    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:
        blockBytes = 16; return true;

    default: blockBytes = 0; return false;
    }
}

inline bool BytesPerPixel4(DXGI_FORMAT f, UINT& bpp)
{
    switch (f)
    {
    case DXGI_FORMAT_R8_UNORM:             bpp = 1;  return true;
    case DXGI_FORMAT_R8G8_UNORM:           bpp = 2;  return true;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:  bpp = 4;  return true;
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:  bpp = 4;  return true;
    case DXGI_FORMAT_R16G16B16A16_FLOAT:   bpp = 8;  return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT:   bpp = 16; return true;
    default: bpp = 0; return false;
    }
}


inline DXGI_FORMAT TypelessToTypedSRV(DXGI_FORMAT f)
{
    switch (f)
    {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:  return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS:  return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_BC1_TYPELESS:       return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:       return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:       return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:       return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:       return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:       return DXGI_FORMAT_BC7_UNORM;
    default: return f; 
    }
}

inline bool ComputePitch(DXGI_FORMAT fmt, UINT w, UINT h, UINT& rowPitch, UINT& slicePitch)
{
    UINT blockBytes = 0, bpp = 0;
    if (IsBCFormat(fmt, blockBytes))
    {
        
        UINT nbw = std::max<UINT>(1, (w + 3) / 4);
        UINT nbh = std::max<UINT>(1, (h + 3) / 4);
        rowPitch = nbw * blockBytes;
        slicePitch = rowPitch * nbh;
        return true;
    }
    if (BytesPerPixel4(fmt, bpp))
    {
        rowPitch = w * bpp;
        slicePitch = rowPitch * h;
        return true;
    }
    return false; 
}

struct TextureResult
{
    ComPtr<ID3D11Texture2D>           texture;
    ComPtr<ID3D11ShaderResourceView>  srv;
};






























































































