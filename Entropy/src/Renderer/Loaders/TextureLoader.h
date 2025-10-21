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

static inline DXGI_FORMAT TypelessToTypedSRV3D(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:   return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32_TYPELESS:        return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:return DXGI_FORMAT_R32G32B32A32_FLOAT;
        // BC formats if you have them in volumes (D3D11 allows it):
    case DXGI_FORMAT_BC1_TYPELESS:        return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:        return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:        return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:        return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:        return DXGI_FORMAT_BC5_UNORM;
    default:                               return f;
    }
}

static inline bool ComputePitch2D(DXGI_FORMAT fmt, UINT w, UINT h,
    UINT& rowPitch, UINT& slicePitch) {
    auto bytesPerPixel = [](DXGI_FORMAT f)->UINT {
        switch (f) {
        case DXGI_FORMAT_R8_UNORM:               return 1;
        case DXGI_FORMAT_R8G8_UNORM:             return 2;
        case DXGI_FORMAT_R8G8B8A8_UNORM:         return 4;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:     return 8;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:     return 16;
        default: return 0;
        }
        };
    auto bcBytesPerBlock = [](DXGI_FORMAT f)->UINT {
        switch (f) {
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC4_UNORM: return 8;
        case DXGI_FORMAT_BC2_UNORM:
        case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC5_UNORM: return 16;
        default: return 0;
        }
        };

    if (UINT b = bcBytesPerBlock(fmt)) {
        const UINT bw = std::max<UINT>(1, (w + 3) / 4);
        const UINT bh = std::max<UINT>(1, (h + 3) / 4);
        rowPitch = bw * b;
        slicePitch = rowPitch * bh;
        return true;
    }

    if (UINT p = bytesPerPixel(fmt)) {
        rowPitch = w * p;
        slicePitch = rowPitch * h;
        return true;
    }

    return false; // unsupported format in this helper
}

static std::optional<Texture3DPayload>
BuildTexture3DPayloadFromTag(TagHash textureTag)
{
    // Parse header (you already have bin::parse<>; extend your STextureHeader to include depth & dimension).
    auto header = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);

    if (header.width == 0 || header.height == 0 || header.depth == 0)
        return std::nullopt;
    auto data_tag = TagHash(textureTag.reference);
    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV3D(fileFmt);

    const uint8_t* src = static_cast<const uint8_t*>(data_tag.data);
    size_t         srcSize = data_tag.size;

    Texture3DPayload tp{};
    ZeroMemory(&tp.desc, sizeof(tp.desc));
    tp.desc.Width = header.width;
    tp.desc.Height = header.height;
    tp.desc.Depth = header.depth;
    tp.desc.MipLevels = std::max<UINT>(1, header.mipCount);
    tp.desc.Format = texFmt;
    tp.desc.Usage = D3D11_USAGE_DEFAULT;
    tp.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tp.desc.CPUAccessFlags = 0;
    tp.desc.MiscFlags = 0;

    // Figure out how many mips are actually present in the blob (no over-reads)
    UINT w = header.width, h = header.height, d = header.depth;
    size_t consumed = 0;
    UINT storedMipCount = 0;

    for (UINT mip = 0; mip < tp.desc.MipLevels; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch))
            return std::nullopt;

        const size_t mipBytes = size_t(slicePitch) * size_t(d);   // all Z slices
        if (consumed + mipBytes > srcSize) break;

        consumed += mipBytes;
        ++storedMipCount;

        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        d = std::max(1u, d >> 1);
    }

    // Clamp mip levels to what we actually have
    tp.desc.MipLevels = storedMipCount;

    // Copy the bytes we will reference from subresource pointers
    tp.data.assign(src, src + consumed);

    // Build subresources (one per mip)
    tp.subresources.reserve(tp.desc.MipLevels);

    const uint8_t* base = tp.data.data();
    size_t offset = 0;
    w = header.width; h = header.height; d = header.depth;

    for (UINT mip = 0; mip < tp.desc.MipLevels; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch)) return std::nullopt;

        const size_t mipBytes = size_t(slicePitch) * size_t(d);

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + offset;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = slicePitch; // bytes per 2D slice
        tp.subresources.push_back(s);

        offset += mipBytes;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        d = std::max(1u, d >> 1);
    }

    return tp;
}


static std::optional<Texture2DPayload>
BuildTexturePayloadFromTag(TagHash textureTag)
{
    // Parse header
    const auto header =
        bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);

    if (header.width == 0 || header.height == 0)
        return std::nullopt;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV(fileFmt);   // your helper

    // Input bytes
    const auto* src = static_cast<const uint8_t*>(header.large_buffer.data);
    const size_t srcLen = header.large_buffer.size;
    if (!src || srcLen == 0)
        return std::nullopt;

    // 3D or 2D?
    const UINT depth0 = std::max<UINT>(1, header.depth);  // if header.depth is missing, set to 1 earlier
    const bool is3D = (depth0 > 1);

    // Pitch helpers (you already have ComputePitch(fmt, w, h, rowPitch, slicePitch))
    auto bytesForMip2D = [&](UINT w, UINT h) -> UINT {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return 0u;
        return slicePitch;
        };
    auto bytesForMip3D = [&](UINT w, UINT h, UINT d) -> UINT {
        UINT rowPitch = 0, slicePitch = 0;  // slicePitch = bytes for ONE z-slice
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return 0u;
        // A 3D mip contains d slices laid back-to-back
        return slicePitch * d;
        };

    // Determine how many mips are actually present in the blob
    const UINT requestedMips = std::max<UINT>(1, header.mipCount);

    UINT w = header.width;
    UINT h = header.height;
    UINT d = depth0;

    size_t consumed = 0;
    UINT storedMipCount = 0;

    while (storedMipCount < requestedMips) {
        UINT need = is3D ? bytesForMip3D(w, h, d) : bytesForMip2D(w, h);
        if (need == 0 || consumed + need > srcLen) break;

        consumed += need;
        ++storedMipCount;

        // next mip
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        if (is3D) d = std::max(1u, d >> 1);
    }
    if (storedMipCount == 0) return std::nullopt;

    // Build payload
    Texture2DPayload tp{};
    ZeroMemory(&tp.desc, sizeof(tp.desc));
    tp.desc.Width = header.width;
    tp.desc.Height = header.height;
    tp.desc.MipLevels = storedMipCount;            // what we actually have
    tp.desc.ArraySize = std::max<UINT>(1, header.arraySize);  // or 1 if no arrays
    tp.desc.Format = texFmt;
    tp.desc.SampleDesc.Count = 1;
    tp.desc.SampleDesc.Quality = 0;
    tp.desc.Usage = D3D11_USAGE_DEFAULT;
    tp.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    tp.desc.CPUAccessFlags = 0;
    tp.desc.MiscFlags = 0;
      


    // Copy the pixel bytes we will reference from subresource pointers
    tp.data.assign(src, src + consumed);

    // Build subresources (point into tp.data)
    tp.subresources.clear();
    tp.subresources.reserve(storedMipCount);

    const uint8_t* base = tp.data.data();
    size_t offset = 0;

    w = header.width; h = header.height; d = depth0;

    for (UINT mip = 0; mip < storedMipCount; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch))
            return std::nullopt;

        const UINT bytesThisMip = is3D ? slicePitch * d : slicePitch;
        if (offset + bytesThisMip > tp.data.size())
            return std::nullopt;

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + offset;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = slicePitch;   // NOTE: for 3D this is per-slice; runtime reads d slices
        tp.subresources.push_back(s);

        offset += bytesThisMip;

        // next mip
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        if (is3D) d = std::max(1u, d >> 1);
    }

    // Optional: if you want the full mip chain even when the file stores fewer:
    // - set desc*.MipLevels = header.mipCount;
    // - add D3D11_BIND_RENDER_TARGET and D3D11_RESOURCE_MISC_GENERATE_MIPS
    // - upload only mip 0 in subresources, then call GenerateMips on the SRV after create.

    return tp;
}
