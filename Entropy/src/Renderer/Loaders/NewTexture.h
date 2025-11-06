// TextureLoader.h
#pragma once
#include <atomic>
#include <vector>
#include <cstdint>
#include <optional>
#include <string>
#include <wrl.h>
#include <d3d11.h>
#include <dxgiformat.h>

#include "TigerEngine/tag.h"                 // TagHash
#include "Runtime/Assets/RuntimeAssetRegistry.h"
//#include "TigerEngine/Technique/texture.h"

// TextureHelpers.h (include this from AssetSystem.cpp)

#pragma once
#include <d3d11.h>
#include <dxgiformat.h>
#include <algorithm>


inline bool IsBC(DXGI_FORMAT f, UINT& blockBytes) {
    switch (f) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: blockBytes = 8;  return true;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:             blockBytes = 16; return true;
    default: blockBytes = 0; return false;
    }
}

inline bool BytesPerPixel(DXGI_FORMAT f, UINT& bpp) {
    switch (f) {
    case DXGI_FORMAT_R8_UNORM: bpp = 1;  return true;
    case DXGI_FORMAT_R8G8_UNORM: bpp = 2; return true;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: bpp = 4; return true;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT: bpp = 8; return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: bpp = 16; return true;
    default: bpp = 0; return false;
    }
}

// rowPitch & slicePitch for a 2D slice
inline bool ComputePitch2D(DXGI_FORMAT fmt, UINT w, UINT h,
    UINT& rowPitch, UINT& slicePitch)
{
    UINT block = 0, bpp = 0;
    if (IsBC(fmt, block)) {
        const UINT nbw = std::max<UINT>(1, (w + 3) / 4);
        const UINT nbh = std::max<UINT>(1, (h + 3) / 4);
        rowPitch = nbw * block;
        slicePitch = rowPitch * nbh;
        return true;
    }
    if (BytesPerPixel(fmt, bpp)) {
        rowPitch = w * bpp;
        slicePitch = rowPitch * h;
        return true;
    }
    rowPitch = slicePitch = 0;
    return false;
}

// typeless ? typed for SRV (preserves *_SRGB if already typed)
inline DXGI_FORMAT TypelessToTypedSRV(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS: return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_B8G8R8A8_TYPELESS: return DXGI_FORMAT_B8G8R8A8_UNORM;
    case DXGI_FORMAT_BC1_TYPELESS:      return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:      return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:      return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:      return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:      return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:      return DXGI_FORMAT_BC7_UNORM;
    default:                            return f;
    }
}


// ---- 1:1 global toggle (Rust: pub static LOW_RES) ----
inline std::atomic<bool> LOW_RES{ false };

enum class TextureHandleKind { Tex2D, TexCube, Tex3D };

struct TextureHandle
{
    TextureHandleKind kind{};
    Microsoft::WRL::ComPtr<ID3D11Texture2D>  tex2D;   // also used for cube
    Microsoft::WRL::ComPtr<ID3D11Texture3D>  tex3D;
};

struct LoadedTexture
{
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> view;
    TextureHandle handle;
    DXGI_FORMAT format; // mirrors Rust Texture { format: DxgiFormat }
};

// --- helpers equivalent to DxgiFormat::calculate_pitch(width, height) ---
inline bool isBC(DXGI_FORMAT f, UINT& blockBytes)
{
    switch (f) {
    case DXGI_FORMAT_BC1_UNORM:
    case DXGI_FORMAT_BC1_UNORM_SRGB:
    case DXGI_FORMAT_BC4_UNORM:
    case DXGI_FORMAT_BC4_SNORM: blockBytes = 8;  return true;
    case DXGI_FORMAT_BC2_UNORM:
    case DXGI_FORMAT_BC2_UNORM_SRGB:
    case DXGI_FORMAT_BC3_UNORM:
    case DXGI_FORMAT_BC3_UNORM_SRGB:
    case DXGI_FORMAT_BC5_UNORM:
    case DXGI_FORMAT_BC5_SNORM:
    case DXGI_FORMAT_BC6H_UF16:
    case DXGI_FORMAT_BC6H_SF16:
    case DXGI_FORMAT_BC7_UNORM:
    case DXGI_FORMAT_BC7_UNORM_SRGB:   blockBytes = 16; return true;
    default: blockBytes = 0; return false;
    }
}
inline bool bppFor(DXGI_FORMAT f, UINT& bpp)
{
    switch (f) {
    case DXGI_FORMAT_R8_UNORM: bpp = 1;  return true;
    case DXGI_FORMAT_R8G8_UNORM: bpp = 2; return true;
    case DXGI_FORMAT_R8G8B8A8_UNORM:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
    case DXGI_FORMAT_B8G8R8A8_UNORM:
    case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB: bpp = 4; return true;
    case DXGI_FORMAT_R16G16B16A16_UNORM:
    case DXGI_FORMAT_R16G16B16A16_FLOAT: bpp = 8; return true;
    case DXGI_FORMAT_R32G32B32A32_FLOAT: bpp = 16; return true;
    default: bpp = 0; return false;
    }
}
// returns (rowPitch, slicePitch)
inline std::pair<size_t, size_t> calculate_pitch(DXGI_FORMAT fmt, size_t w, size_t h)
{
    UINT block = 0, bpp = 0;
    if (isBC(fmt, block)) {
        const size_t nbw = std::max<size_t>(1, (w + 3) / 4);
        const size_t nbh = std::max<size_t>(1, (h + 3) / 4);
        const size_t row = nbw * block;
        return { row, row * nbh };
    }
    if (bppFor(fmt, bpp)) {
        const size_t row = w * bpp;
        return { row, row * h };
    }
    return { 0, 0 };
}

inline UINT calc_dx_subresource(UINT mip, UINT arraySlice, UINT mipLevels)
{
    // matches Rust calc_dx_subresource(mip, e, mip_count)
    return mip + arraySlice * mipLevels;
}

