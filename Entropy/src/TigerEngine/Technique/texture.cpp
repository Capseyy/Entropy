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
    uint32_t mipCount;      // >= 1
    uint32_t dxgiFormat;    // DXGI_FORMAT as uint32_t from file
    // ... anything else you keep
};

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


bool TigerTexture::Initialize(ID3D11Device* device, TagHash textureTag)
{
    if (!device || !textureTag.data) return false;

    const DXGI_FORMAT fileFmt = static_cast<DXGI_FORMAT>(header.dxgiFormat);
    const DXGI_FORMAT texFmt = TypelessToTypedSRV(fileFmt);

    if (!header.width || !header.height) return false;

    // Compute how many mips are actually present in the blob (assuming tight packing)
    auto bytesForMip = [&](UINT w, UINT h)->UINT {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return 0u;
        return slicePitch;
        };

    const size_t dataSize = header.large_buffer.size;   // <-- make sure you have this
    size_t consumed = 0;
    UINT w = header.width, h = header.height;
    UINT storedMipCount = 0;

    while (storedMipCount < std::max<UINT>(1, header.mipCount)) {
        UINT sz = bytesForMip(w, h);
        if (!sz || consumed + sz > dataSize) break;  // not enough bytes for another mip
        consumed += sz;
        ++storedMipCount;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    if (storedMipCount == 0) return false; // corrupt blob

    // Decide what to create:
    //   - If you want only what you have: create with storedMipCount.
    //   - If you want full chain: create with full mip count and autogen the rest later.
    const UINT createMipLevels = storedMipCount; // or = header.mipCount if you’ll autogen

    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = header.width;
    desc.Height = header.height;
    desc.MipLevels = createMipLevels;
    desc.ArraySize = 1;
    desc.Format = texFmt;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

    // Build subresources from the bytes we actually have
    std::vector<D3D11_SUBRESOURCE_DATA> subs;
    subs.reserve(createMipLevels);

    const uint8_t* ptr = static_cast<const uint8_t*>(header.large_buffer.data);
    w = header.width; h = header.height;
    for (UINT mip = 0; mip < createMipLevels; ++mip) {
        UINT rowPitch = 0, slicePitch = 0;
        if (!ComputePitch(texFmt, w, h, rowPitch, slicePitch)) return false;

        D3D11_SUBRESOURCE_DATA s{};
        s.pSysMem = ptr;
        s.SysMemPitch = rowPitch;
        s.SysMemSlicePitch = slicePitch;
        subs.push_back(s);

        ptr += slicePitch;
        w = std::max(1u, w >> 1);
        h = std::max(1u, h >> 1);
    }

    Microsoft::WRL::ComPtr<ID3D11Texture2D> texture;
    HRESULT hr = device->CreateTexture2D(&desc, subs.data(), &texture);
    COM_ERROR_IF_FAILED(hr, "CreateTexture2D failed");

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format = texFmt;
    srvd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvd.Texture2D.MostDetailedMip = 0;
    srvd.Texture2D.MipLevels = desc.MipLevels;

    hr = device->CreateShaderResourceView(texture.Get(), &srvd, &textureView);
    COM_ERROR_IF_FAILED(hr, "CreateShaderResourceView failed");

    // OPTIONAL: if header.mipCount > storedMipCount and you want the full chain:
    //  - Create the texture with full mip count and flags:
    //        desc.BindFlags |= D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
    //        desc.MiscFlags |= D3D11_RESOURCE_MISC_GENERATE_MIPS;
    //  - Upload mip 0 with UpdateSubresource, then call context->GenerateMips(SRV).
    printf("TigerTexture: %ux%u fmt=%u mips=%u (stored %u) size=%zu\n",
		header.width, header.height, texFmt, header.mipCount, storedMipCount, dataSize);

    return true;
}