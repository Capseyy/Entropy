#pragma once
#include "TigerEngine/tag.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include <d3d11.h>
#include <wrl.h>
#include <vector>
#include <algorithm>
#include <cstdint>
#include "Runtime/Assets/Pitch.h"
#undef max
#undef min
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")
#define IDB_ANGlE_LOOKUP 102

HRESULT EnsureWIC();

// File ? SRV (uses WIC: png/jpg/tiff/…)
HRESULT LoadTextureFromFileWIC(
    ID3D11Device* dev,
    ID3D11DeviceContext* ctx,        // can be null; used if we stage/convert
    const wchar_t* path,
    bool srgb,
    ID3D11ShaderResourceView** outSRV);

// Memory ? SRV (for baked/embedded bytes)
HRESULT LoadTextureFromMemoryWIC(
    ID3D11Device* dev,
    const void* bytes, size_t size,
    bool srgb,
    ID3D11ShaderResourceView** outSRV);

// Raw RGBA8 ? SRV (utility)
HRESULT CreateSRVFromRGBA8(
    ID3D11Device* dev,
    const uint8_t* rgba, UINT w, UINT h, bool srgb,
    ID3D11ShaderResourceView** outSRV);

static HRESULT LoadEmbeddedTextureSRV(ID3D11Device* dev, UINT resId, bool srgb,
    ID3D11ShaderResourceView** outSRV)
{
    *outSRV = nullptr;
    HMODULE hMod = GetModuleHandleW(nullptr);

    // Try RCDATA, then PNG
    const wchar_t* kinds[] = { RT_RCDATA, L"PNG" };
    HRSRC hRes = nullptr;
    for (auto k : kinds) {
        hRes = FindResourceW(hMod, MAKEINTRESOURCEW(resId), k);
        if (hRes) break;
    }
    if (!hRes) return HRESULT_FROM_WIN32(GetLastError());

    HGLOBAL hMem = LoadResource(hMod, hRes);
    if (!hMem) return HRESULT_FROM_WIN32(GetLastError());

    DWORD size = SizeofResource(hMod, hRes);
    const void* ptr = LockResource(hMem);
    if (!ptr || !size) return E_FAIL;

    return LoadTextureFromMemoryWIC(dev, ptr, size, srgb, outSRV);
}


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


struct TextureResult
{
    ComPtr<ID3D11Texture2D>           texture;
    ComPtr<ID3D11ShaderResourceView>  srv;
};

static std::optional<Texture3DPayload>
BuildTexture3DPayloadFromTag(TagHash textureTag)
{
    const auto hdr = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);
    if (!hdr.width || !hdr.height || hdr.depth <= 1) return std::nullopt;

    // Find pixel bytes (prefer large buffer)
    const uint8_t* src = nullptr; size_t srcSize = 0;
    if (hdr.large_buffer.data && hdr.large_buffer.size) {
        src = static_cast<const uint8_t*>(hdr.large_buffer.data);
        srcSize = hdr.large_buffer.size;
    }
    else {
        TagHash payload(textureTag.reference);
        payload.getData();                 // <<— make data/size valid
        src = payload.data;
        srcSize = payload.size;
    }
    if (!src || !srcSize) return std::nullopt;

    const DXGI_FORMAT resFmt = static_cast<DXGI_FORMAT>(hdr.dxgiFormat);
    const DXGI_FORMAT pitchFmt = NormalizeForPitch(TypelessToTypedSRV3D(resFmt));

    Texture3DPayload tp{};
    tp.desc.Width = hdr.width;
    tp.desc.Height = hdr.height;
    tp.desc.Depth = hdr.depth;
    tp.desc.Format = resFmt;            // resource can stay typeless
    tp.desc.MipLevels = std::max<UINT>(1, hdr.mipCount);
    tp.desc.Usage = D3D11_USAGE_DEFAULT;
    tp.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // Count how many mips actually fit
    UINT w = hdr.width, h = hdr.height, d = hdr.depth;
    size_t consumed = 0; UINT stored = 0;
    for (UINT mip = 0; mip < tp.desc.MipLevels; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(pitchFmt, w, h, rowPitch, slicePitch)) return std::nullopt;
        const size_t mipBytes = size_t(slicePitch) * size_t(d);
        if (consumed + mipBytes > srcSize) break;
        consumed += mipBytes; ++stored;
        w = std::max(1u, w >> 1); h = std::max(1u, h >> 1); d = std::max(1u, d >> 1);
    }
    if (!stored) return std::nullopt;
    tp.desc.MipLevels = stored;

    // Copy bytes we’ll reference
    tp.data.assign(src, src + consumed);

    // Build subresources (pSysMem points into tp.data)
    tp.subresources.reserve(stored);
    const uint8_t* base = tp.data.data();
    size_t offset = 0; w = hdr.width; h = hdr.height; d = hdr.depth;

    for (UINT mip = 0; mip < stored; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        ComputePitch2D(pitchFmt, w, h, rowPitch, slicePitch);

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + offset;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = slicePitch;
        tp.subresources.push_back(s);

        offset += size_t(slicePitch) * size_t(d);
        w = std::max(1u, w >> 1); h = std::max(1u, h >> 1); d = std::max(1u, d >> 1);
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
    size_t srcLen;
    unsigned char* src;
    if (header.large_buffer.hash != 0xFFFFFFFF) {
        src = static_cast<uint8_t*>(header.large_buffer.data);
        srcLen = header.large_buffer.size;
    }
    else {
        TagHash texRef = TagHash(textureTag.reference);
        src = static_cast<uint8_t*>(texRef.data);
        srcLen = texRef.size;
    }
    

    // 3D or 2D?
    const UINT depth0 = std::max<UINT>(1, header.depth);  // if header.depth is missing, set to 1 earlier
    const bool is3D = (depth0 > 1);

    // Pitch helpers (you already have ComputePitch(fmt, w, h, rowPitch, slicePitch))
    auto bytesForMip2D = [&](UINT w, UINT h) -> UINT {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch)) return 0u;
        return slicePitch;
        };
    auto bytesForMip3D = [&](UINT w, UINT h, UINT d) -> UINT {
        UINT rowPitch = 0, slicePitch = 0;  // slicePitch = bytes for ONE z-slice
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch)) return 0u;
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
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch))
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
