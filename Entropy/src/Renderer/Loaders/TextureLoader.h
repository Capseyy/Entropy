#pragma once
#include "TigerEngine/tag.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#undef max
#undef min
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")



inline std::pair<size_t, size_t>
calculate_pitch(DXGI_FORMAT fmt, size_t width, size_t height)
{
    switch (fmt)
    {
        // --- BC 4x4, 8 bytes per block ---
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

    // --- BC 4x4, 16 bytes per block ---
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


using Microsoft::WRL::ComPtr;


// Returns true if BCn (block compressed). Sets blockBytes=8 for BC1/BC4, 16 otherwise.
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

inline bool BytesPerPixel(DXGI_FORMAT f, UINT& bpp)
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

// If your header sometimes contains TYPELESS formats, pick a sane typed SRV format.
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
    default: return f; // already typed
    }
}

inline bool ComputePitch(DXGI_FORMAT fmt, UINT w, UINT h, UINT& rowPitch, UINT& slicePitch)
{
    UINT blockBytes = 0, bpp = 0;
    if (IsBCFormat(fmt, blockBytes))
    {
        // BC uses 4x4 blocks; dimensions below 4 still occupy 1 block.
        UINT nbw = std::max<UINT>(1, (w + 3) / 4);
        UINT nbh = std::max<UINT>(1, (h + 3) / 4);
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
    return false; // unsupported/planar format
}

struct TextureResult
{
    ComPtr<ID3D11Texture2D>           texture;
    ComPtr<ID3D11ShaderResourceView>  srv;
};


static std::optional<TexturePayload> BuildTexturePayloadFromTag(TagHash textureTag)
{
    // Parse your header from the tag
    auto header = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);

    // Validate basic fields
    if (header.width == 0 || header.height == 0) return std::nullopt;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV(fileFmt); // your helper

    // We need to compute how many mips are *actually present* in the blob
    auto bytesForMip = [&](UINT w, UINT h) -> UINT {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return 0u;
        return slicePitch;
        };

    // Locate the pixel bytes. Your header already exposes a "large_buffer" with .data/.size
    const uint8_t* src = static_cast<const uint8_t*>(header.large_buffer.data);
    size_t         srcSize = header.large_buffer.size;
    if (!src || srcSize == 0) return std::nullopt;

    // Count how many mips are tightly packed in the blob
    UINT w = header.width, h = header.height;
    size_t consumed = 0;
    UINT storedMipCount = 0;
    const UINT requestedMips = std::max<UINT>(1, header.mipCount);

    while (storedMipCount < requestedMips) {
        const UINT sz = bytesForMip(w, h);
        if (sz == 0 || consumed + sz > srcSize) break;   // no room for next mip
        consumed += sz;
        ++storedMipCount;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }
    if (storedMipCount == 0) return std::nullopt;

    // Build the payload
    TexturePayload tp{};
    ZeroMemory(&tp.desc, sizeof(tp.desc));
    tp.desc.Width = header.width;
    tp.desc.Height = header.height;
    tp.desc.MipLevels = storedMipCount;       // only what we actually have
    tp.desc.ArraySize = 1;                    // adjust if your header has arrays
    tp.desc.Format = texFmt;
    tp.desc.SampleDesc.Count = 1;
    tp.desc.SampleDesc.Quality = 0;
    tp.desc.Usage = D3D11_USAGE_DEFAULT;
    tp.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tp.desc.CPUAccessFlags = 0;
    tp.desc.MiscFlags = 0;

    // Copy the pixel bytes we will reference from subresource pointers
    tp.data.assign(src, src + consumed);

    // Build subresources; their pSysMem pointers must point *into* tp.data
    tp.subresources.reserve(tp.desc.MipLevels);

    const uint8_t* base = tp.data.data();
    size_t offset = 0;
    w = header.width; h = header.height;

    for (UINT mip = 0; mip < tp.desc.MipLevels; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return std::nullopt;
        if (offset + slicePitch > tp.data.size()) return std::nullopt;

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + offset;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = slicePitch;
        tp.subresources.push_back(s);

        offset += slicePitch;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    // Optional: if header.mipCount > storedMipCount and you want full chain,
    // set tp.desc.MipLevels = header.mipCount and add flags:
    //   tp.desc.BindFlags |= D3D11_BIND_RENDER_TARGET;
    //   tp.desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    // Your createTexture_ can then upload mip 0 and call GenerateMips.

    return tp;
}
