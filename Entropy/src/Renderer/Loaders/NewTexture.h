
#pragma once

#include <atomic>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <algorithm>

#include <wrl.h>
#include <d3d11.h>
#include <dxgiformat.h>

#include "TigerEngine/tag.h"               
#include "Runtime/Assets/RuntimeAssetRegistry.h"

inline std::atomic<bool> LOW_RES{ false };


enum class TextureHandleKind { Tex2D, TexCube, Tex3D };

struct TextureHandle
{
    TextureHandleKind kind{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex2D; // also used for cube
    Microsoft::WRL::ComPtr<ID3D11Texture3D> tex3D;
};

struct LoadedTexture
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    TextureHandle handle;
    DXGI_FORMAT format{};
};


inline bool IsBlockCompressed(DXGI_FORMAT f, UINT& blockBytes)
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

    default:
        blockBytes = 0;  return false;
    }
}

inline bool BytesPerPixel(DXGI_FORMAT f, UINT& bpp)
{
    switch (f)
    {
        // 8-bit
    case DXGI_FORMAT_R8_UNORM:
    case DXGI_FORMAT_R8_SNORM:
    case DXGI_FORMAT_R8_UINT:
    case DXGI_FORMAT_R8_SINT:
        bpp = 1; return true;

        // 16-bit
    case DXGI_FORMAT_R16_UNORM:
    case DXGI_FORMAT_R16_SNORM:
    case DXGI_FORMAT_R16_UINT:
    case DXGI_FORMAT_R16_SINT:
    case DXGI_FORMAT_R16_FLOAT:
        bpp = 2; return true;

        // 16-bit x2  (THIS FIXES YOUR RG16_UNORM CASE)
    case DXGI_FORMAT_R16G16_UNORM:
    case DXGI_FORMAT_R16G16_SNORM:
    case DXGI_FORMAT_R16G16_UINT:
    case DXGI_FORMAT_R16G16_SINT:
    case DXGI_FORMAT_R16G16_FLOAT:
        bpp = 4; return true;

        // 32-bit
    case DXGI_FORMAT_R32_UINT:
    case DXGI_FORMAT_R32_SINT:
    case DXGI_FORMAT_R32_FLOAT:
        bpp = 4; return true;

        // 32-bit x2
    case DXGI_FORMAT_R32G32_UINT:
    case DXGI_FORMAT_R32G32_SINT:
    case DXGI_FORMAT_R32G32_FLOAT:
        bpp = 8; return true;

        // 32-bit x3
    case DXGI_FORMAT_R32G32B32_UINT:
    case DXGI_FORMAT_R32G32B32_SINT:
    case DXGI_FORMAT_R32G32B32_FLOAT:
        bpp = 12; return true;

        // 32-bit x4
    case DXGI_FORMAT_R32G32B32A32_UINT:
    case DXGI_FORMAT_R32G32B32A32_SINT:
    case DXGI_FORMAT_R32G32B32A32_FLOAT:
        bpp = 16; return true;

        // common color formats
    case DXGI_FORMAT_R8G8_UNORM:
    case DXGI_FORMAT_R8G8_SNORM:
    case DXGI_FORMAT_R8G8_UINT:
    case DXGI_FORMAT_R8G8_SINT:
        bpp = 2; return true;

    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_R8G8B8A8_SNORM:
    case DXGI_FORMAT_R8G8B8A8_UINT:
    case DXGI_FORMAT_R8G8B8A8_SINT:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
        bpp = 4; return true;

    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_SNORM:
    case DXGI_FORMAT_R16G16B16A16_UINT:
    case DXGI_FORMAT_R16G16B16A16_SINT:
    case DXGI_FORMAT_R16G16B16A16_FLOAT:
        bpp = 8; return true;

    default:
        bpp = 0; return false;
    }
}

inline bool ComputePitch2D(DXGI_FORMAT fmt, UINT w, UINT h, UINT& rowPitch, UINT& slicePitch)
{
    UINT blockBytes = 0, bpp = 0;

    if (IsBlockCompressed(fmt, blockBytes))
    {
        // BC formats are in 4x4 blocks
        const UINT nbw = std::max<UINT>(1, (w + 3) / 4);
        const UINT nbh = std::max<UINT>(1, (h + 3) / 4);
        rowPitch = nbw * blockBytes;
        slicePitch = rowPitch * nbh;
        return true;
    }

    if (BytesPerPixel(fmt, bpp))
    {
        rowPitch = w * bpp;
        slicePitch = rowPitch * h;
        return true;
    }

    rowPitch = slicePitch = 0;
    return false;
}

inline std::pair<size_t, size_t> calculate_pitch(DXGI_FORMAT fmt, size_t w, size_t h)
{
    UINT row = 0, slice = 0;
    if (!ComputePitch2D(fmt, static_cast<UINT>(w), static_cast<UINT>(h), row, slice))
        return { 0, 0 };
    return { static_cast<size_t>(row), static_cast<size_t>(slice) };
}

inline DXGI_FORMAT TypelessToTypedSRV(DXGI_FORMAT f)
{
    switch (f)
    {
        // 8-bit color
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;

        // BC
    case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:      return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:      return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;

        // R/RG 16-bit
    case DXGI_FORMAT_R16_TYPELESS:      return DXGI_FORMAT_R16_UNORM;
    case DXGI_FORMAT_R16G16_TYPELESS:   return DXGI_FORMAT_R16G16_UNORM;

        // R 32-bit
    case DXGI_FORMAT_R32_TYPELESS:      return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32_TYPELESS:   return DXGI_FORMAT_R32G32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    default:
        return f;
    }
}

inline UINT calc_dx_subresource(UINT mip, UINT arraySlice, UINT mipLevels)
{
    // matches D3D11CalcSubresource(mip, arraySlice, mipLevels) for 2D
    return mip + arraySlice * mipLevels;
}
