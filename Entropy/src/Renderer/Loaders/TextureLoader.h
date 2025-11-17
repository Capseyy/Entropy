#pragma once

// ===== public deps =====
#include "TigerEngine/tag.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/Pitch.h" // if you keep extra pitch helpers there; otherwise not required

// ===== sdk =====
#include <d3d11.h>
#include <dxgiformat.h>
#include <wrl.h>

// ===== std =====
#include <vector>
#include <cstdint>
#include <algorithm>
#include <optional>
#include <cstring>
#include "NewTexture.h"

#undef max
#undef min
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

// =====================================================================================
// Small helper namespace (inline functions; single definition; no ODR issues)
// =====================================================================================
namespace entropy_tex
{

    inline bool IsBCFormat(DXGI_FORMAT f, UINT& blockBytes)
    {
        switch (f)
        {
        case DXGI_FORMAT_BC1_TYPELESS:
        case DXGI_FORMAT_BC1_UNORM:
        case DXGI_FORMAT_BC1_UNORM_SRGB:
        case DXGI_FORMAT_BC4_TYPELESS:
        case DXGI_FORMAT_BC4_UNORM:
        case DXGI_FORMAT_BC4_SNORM:
            blockBytes = 8;  return true;

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
            blockBytes = 16; return true;

        default: blockBytes = 0; return false;
        }
    }

    inline bool BytesPerPixel(DXGI_FORMAT f, UINT& bpp) {
        switch (f) {
        case DXGI_FORMAT_R8_UNORM:                       bpp = 1;  return true;
        case DXGI_FORMAT_R8G8_UNORM:                     bpp = 2;  return true;
        case DXGI_FORMAT_R8G8B8A8_UNORM:
        case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
        case DXGI_FORMAT_B8G8R8A8_UNORM:
        case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:            bpp = 4;  return true;
        case DXGI_FORMAT_R16G16B16A16_FLOAT:
        case DXGI_FORMAT_R16G16B16A16_UNORM:             bpp = 8;  return true;
        case DXGI_FORMAT_R32G32B32A32_FLOAT:             bpp = 16; return true;
        default:                                         bpp = 0;  return false;
        }
    }

    // Compute pitch for one 2D slice (works also for array/3D slice)
    inline bool ComputePitch2D(DXGI_FORMAT fmt, UINT w, UINT h, UINT& rowPitch, UINT& slicePitch)
    {
        UINT blockBytes = 0, bpp = 0;
        if (IsBCFormat(fmt, blockBytes)) {
            const UINT nbw = std::max<UINT>(1, (w + 3) / 4);
            const UINT nbh = std::max<UINT>(1, (h + 3) / 4);
            rowPitch = nbw * blockBytes;
            slicePitch = rowPitch * nbh;
            return true;
        }
        if (entropy_tex::BytesPerPixel(fmt, bpp)) {
            rowPitch = w * bpp;
            slicePitch = rowPitch * h;
            return true;
        }
        rowPitch = slicePitch = 0;
        return false; // planar formats not handled here
    }

    // typeless ? typed for SRV; preserves *_SRGB if already typed
    inline DXGI_FORMAT TypelessToTypedSRV(DXGI_FORMAT f)
    {
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
} // namespace entropy_tex

using Microsoft::WRL::ComPtr;

// =====================================================================================
// BuildTexture3DPayloadFromTag  (returns all stored mips for 3D)
// =====================================================================================
static std::optional<Texture3DPayload>
BuildTexture3DPayloadFromTag(TagHash textureTag)
{
    const auto hdr = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);
    if (!hdr.width || !hdr.height || hdr.depth <= 1) return std::nullopt;

    // Source bytes: prefer large_buffer, else reference
    const uint8_t* src = nullptr; size_t srcSize = 0;
    if (hdr.large_buffer.data && hdr.large_buffer.size) {
        src = static_cast<const uint8_t*>(hdr.large_buffer.data);
        srcSize = hdr.large_buffer.size;
    }
    else {
        TagHash ref(textureTag.reference);
        if (!ref.data || !ref.size) return std::nullopt;
        src = static_cast<const uint8_t*>(ref.data);
        srcSize = ref.size;
    }
    if (!src || !srcSize) return std::nullopt;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(hdr.dxgiFormat);
    const DXGI_FORMAT pitchFmt = entropy_tex::TypelessToTypedSRV(fileFmt);

    Texture3DPayload tp{};
    tp.desc.Width = hdr.width;
    tp.desc.Height = hdr.height;
    tp.desc.Depth = hdr.depth;
    tp.desc.MipLevels = std::max<UINT>(1, hdr.mipCount); // header’s advertised mips
    tp.desc.Format = fileFmt;
    tp.desc.Usage = D3D11_USAGE_DEFAULT;
    tp.desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // Determine how many mips are truly present
    UINT w = hdr.width, h = hdr.height, d = hdr.depth;
    size_t consumed = 0; UINT stored = 0;
    for (UINT mip = 0; mip < tp.desc.MipLevels; ++mip) {
        UINT rp = 0, sp = 0;
        if (!entropy_tex::ComputePitch2D(pitchFmt, w, h, rp, sp)) break;
        const size_t mipBytes = size_t(sp) * size_t(d);
        if (consumed + mipBytes > srcSize) break;
        consumed += mipBytes;
        ++stored;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        d = std::max(1u, d >> 1);
    }
    if (!stored) return std::nullopt;

    tp.desc.MipLevels = stored;

    // Copy only the bytes we reference
    tp.data.assign(src, src + consumed);

    // Build subresources
    tp.subresources.clear();
    tp.subresources.reserve(stored);
    const uint8_t* base = tp.data.data();
    size_t offset = 0; w = hdr.width; h = hdr.height; d = hdr.depth;

    for (UINT mip = 0; mip < stored; ++mip) {
        UINT rp = 0, sp = 0;
        entropy_tex::ComputePitch2D(pitchFmt, w, h, rp, sp);

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + offset;
        s.SysMemPitch = rp;
        s.SysMemSlicePitch = sp; // one z-slice
        tp.subresources.push_back(s);

        offset += size_t(sp) * size_t(d);
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
        d = std::max(1u, d >> 1);
    }

    return tp;
}


static std::optional<Texture2DPayload>
BuildTextureCubePayloadFromTag(TagHash textureTag)
{
    const auto header = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);
    if (!header.width || !header.height) return std::nullopt;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV(fileFmt);

    // Pull bytes: same rule as BuildTexturePayloadFromTag
    std::vector<uint8_t> bytes;
    if (header.large_buffer.data && header.large_buffer.size)
    {
        bytes.insert(bytes.end(),
            (const uint8_t*)header.large_buffer.data,
            (const uint8_t*)header.large_buffer.data + header.large_buffer.size);

        TagHash ref(textureTag.reference);
        if (ref.data && ref.size)
            bytes.insert(bytes.end(), (const uint8_t*)ref.data, (const uint8_t*)ref.data + ref.size);
    }
    else
    {
        TagHash ref(textureTag.reference);
        if (!ref.data || !ref.size) return std::nullopt;
        bytes.insert(bytes.end(), (const uint8_t*)ref.data, (const uint8_t*)ref.data + ref.size);
    }

    const bool haveLarge = (header.large_buffer.data && header.large_buffer.size);
    const UINT targetMips = haveLarge ? std::max<UINT>(1, header.mipCount) : 1;

    UINT storedMips = 0;
    size_t off = 0;
    UINT w = header.width, h = header.height;

    for (UINT mip = 0; mip < targetMips; ++mip)
    {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch))
            break;

        const size_t oneFaceBytes = slicePitch;
        const size_t mipBytes = oneFaceBytes * 6;

        if (off + mipBytes > bytes.size())
            break;

        off += mipBytes;
        ++storedMips;

        w = std::max(1u, w >> 1u);
        h = std::max(1u, h >> 1u);
    }

    if (!storedMips)
        return std::nullopt;

    // --- Build payload ---
    Texture2DPayload tp{};
    tp.data = std::move(bytes);

    D3D11_TEXTURE2D_DESC& d = tp.desc;
    ZeroMemory(&d, sizeof(d));
    d.Width = header.width;
    d.Height = header.height;
    d.MipLevels = storedMips;
    d.ArraySize = 6;                      
    d.Format = texFmt;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags = 0;
    d.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    // --- Subresources: [mip][face] ---
    tp.subresources.reserve(storedMips * 6);

    const uint8_t* base = tp.data.data();
    size_t cursor = 0;
    w = header.width;
    h = header.height;

    for (UINT mip = 0; mip < storedMips; ++mip)
    {
        UINT rowPitch = 0, slicePitch = 0;
        ComputePitch2D(texFmt, w, h, rowPitch, slicePitch);

        for (UINT face = 0; face < 6; ++face)
        {
            if (cursor + slicePitch > tp.data.size())
                break;

            D3D11_SUBRESOURCE_DATA s{};
            s.pSysMem = base + cursor;
            s.SysMemPitch = rowPitch;
            s.SysMemSlicePitch = 0;
            tp.subresources.push_back(s);

            cursor += slicePitch;
        }

        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    return tp;
}


// =====================================================================================
static std::optional<Texture2DPayload>
BuildTexturePayloadFromTag(TagHash textureTag)
{
    const auto header = bin::parse<STextureHeader>(textureTag.data, textureTag.size, bin::Endian::Little);
    if (!header.width || !header.height) return std::nullopt;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV(fileFmt);

    std::vector<uint8_t> bytes;
    if (header.large_buffer.data && header.large_buffer.size) {
        bytes.insert(bytes.end(),
            (const uint8_t*)header.large_buffer.data,
            (const uint8_t*)header.large_buffer.data + header.large_buffer.size);
        TagHash ref(textureTag.reference);
        if (ref.data && ref.size) {
            bytes.insert(bytes.end(), (const uint8_t*)ref.data, (const uint8_t*)ref.data + ref.size);
        }
    }
    else {
        TagHash ref(textureTag.reference);
        if (!ref.data || !ref.size) return std::nullopt;
        bytes.insert(bytes.end(), (const uint8_t*)ref.data, (const uint8_t*)ref.data + ref.size);
    }

    const bool haveLarge = (header.large_buffer.data && header.large_buffer.size);
    const UINT targetMips = haveLarge ? std::max<UINT>(1, header.mipCount) : 1;

    UINT w = header.width, h = header.height;
    size_t off = 0; UINT stored = 0;
    for (UINT i = 0; i < targetMips; ++i) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch)) break;
        if (off + slicePitch > bytes.size()) break;
        off += slicePitch; ++stored;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    Texture2DPayload tp{};
    tp.data = std::move(bytes);

    D3D11_TEXTURE2D_DESC& d = tp.desc;
    ZeroMemory(&d, sizeof(d));
    d.Width = header.width;
    d.Height = header.height;
    d.MipLevels = targetMips;                
    d.ArraySize = 1;                         
    d.Format = texFmt;
    d.SampleDesc.Count = 1;
    d.Usage = D3D11_USAGE_DEFAULT;
    d.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    d.CPUAccessFlags = 0;
    d.MiscFlags = 0;

    tp.subresources.reserve(stored);
    const uint8_t* base = tp.data.data();
    size_t cursor = 0; w = header.width; h = header.height;
    for (UINT mip = 0; mip < stored; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch2D(texFmt, w, h, rowPitch, slicePitch)) break;
        if (cursor + slicePitch > tp.data.size()) break;

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = base + cursor;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = 0;
        tp.subresources.push_back(s);

        cursor += slicePitch;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    return tp;
}

// =====================================================================================
// Optional WIC helpers (kept declarations only; implement elsewhere if you use them)
// =====================================================================================
HRESULT EnsureWIC();

HRESULT LoadTextureFromFileWIC(
    ID3D11Device* dev,
    ID3D11DeviceContext* ctx,
    const wchar_t* path,
    bool srgb,
    ID3D11ShaderResourceView** outSRV);

HRESULT LoadTextureFromMemoryWIC(
    ID3D11Device* dev,
    const void* bytes, size_t size,
    bool srgb,
    ID3D11ShaderResourceView** outSRV);

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
