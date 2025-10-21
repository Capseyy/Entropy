#pragma once
#pragma once
#include <dxgiformat.h>
#include <algorithm>
#include <cstdint>
#include <d3d11.h>
#include <vector>
#include <memory>
#undef min
#undef max


// ---- normalize typeless/sRGB to typed linear for pitch math ----
inline DXGI_FORMAT NormalizeForPitch(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:
    case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:   return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS: return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32_TYPELESS:          return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS: return DXGI_FORMAT_R32G32B32A32_FLOAT;

    case DXGI_FORMAT_BC1_TYPELESS:
    case DXGI_FORMAT_BC1_UNORM_SRGB:        return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:
    case DXGI_FORMAT_BC2_UNORM_SRGB:        return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:
    case DXGI_FORMAT_BC3_UNORM_SRGB:        return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:          return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:          return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC6H_TYPELESS:         return DXGI_FORMAT_BC6H_UF16;
    case DXGI_FORMAT_BC7_TYPELESS:
    case DXGI_FORMAT_BC7_UNORM_SRGB:        return DXGI_FORMAT_BC7_UNORM;
    default:                                 return f;
    }
}
inline DXGI_FORMAT TypelessToTypedSRV3D(DXGI_FORMAT f) {
    switch (f) {
    case DXGI_FORMAT_R8G8B8A8_TYPELESS:    return DXGI_FORMAT_R8G8B8A8_UNORM;
    case DXGI_FORMAT_R16G16B16A16_TYPELESS:return DXGI_FORMAT_R16G16B16A16_FLOAT;
    case DXGI_FORMAT_R32_TYPELESS:         return DXGI_FORMAT_R32_FLOAT;
    case DXGI_FORMAT_R32G32B32A32_TYPELESS:return DXGI_FORMAT_R32G32B32A32_FLOAT;
    case DXGI_FORMAT_BC1_TYPELESS:         return DXGI_FORMAT_BC1_UNORM;
    case DXGI_FORMAT_BC2_TYPELESS:         return DXGI_FORMAT_BC2_UNORM;
    case DXGI_FORMAT_BC3_TYPELESS:         return DXGI_FORMAT_BC3_UNORM;
    case DXGI_FORMAT_BC4_TYPELESS:         return DXGI_FORMAT_BC4_UNORM;
    case DXGI_FORMAT_BC5_TYPELESS:         return DXGI_FORMAT_BC5_UNORM;
    case DXGI_FORMAT_BC7_TYPELESS:         return DXGI_FORMAT_BC7_UNORM;
    default:                                return f;
    }
}
inline bool ComputePitch2D(DXGI_FORMAT fmtIn, UINT w, UINT h, UINT& rowPitch, UINT& slicePitch)
{
    const DXGI_FORMAT f = NormalizeForPitch(fmtIn);

    auto isBC = [](DXGI_FORMAT F) {
        switch (F) {
        case DXGI_FORMAT_BC1_UNORM: case DXGI_FORMAT_BC2_UNORM: case DXGI_FORMAT_BC3_UNORM:
        case DXGI_FORMAT_BC4_UNORM: case DXGI_FORMAT_BC5_UNORM:
        case DXGI_FORMAT_BC6H_UF16: case DXGI_FORMAT_BC6H_SF16:
        case DXGI_FORMAT_BC7_UNORM: return true; default: return false;
        }
        };
    if (isBC(f)) {
        const bool is8 = (f == DXGI_FORMAT_BC1_UNORM || f == DXGI_FORMAT_BC4_UNORM);
        const UINT bpb = is8 ? 8u : 16u;
        const UINT bw = std::max(1u, (w + 3) / 4);
        const UINT bh = std::max(1u, (h + 3) / 4);
        rowPitch = bw * bpb;
        slicePitch = rowPitch * bh;
        return true;
    }

    auto bpp = [](DXGI_FORMAT F)->UINT {
        switch (F) {
        case DXGI_FORMAT_R8_UNORM: case DXGI_FORMAT_A8_UNORM: case DXGI_FORMAT_R8_UINT: case DXGI_FORMAT_R8_SINT: return 8;
        case DXGI_FORMAT_R8G8_UNORM: case DXGI_FORMAT_R16_UINT: case DXGI_FORMAT_R16_SINT: case DXGI_FORMAT_R16_FLOAT: return 16;
        case DXGI_FORMAT_R8G8B8A8_UNORM: case DXGI_FORMAT_R10G10B10A2_UNORM:
        case DXGI_FORMAT_R11G11B10_FLOAT: case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
        case DXGI_FORMAT_R32_FLOAT: case DXGI_FORMAT_R32_UINT: case DXGI_FORMAT_R32_SINT: return 32;
        case DXGI_FORMAT_R16G16_FLOAT: case DXGI_FORMAT_R16G16_UNORM:
        case DXGI_FORMAT_R16G16_UINT:  case DXGI_FORMAT_R16G16_SINT: return 32;
        case DXGI_FORMAT_R16G16B16A16_FLOAT: case DXGI_FORMAT_R16G16B16A16_UNORM:
        case DXGI_FORMAT_R16G16B16A16_UINT:  case DXGI_FORMAT_R16G16B16A16_SINT: return 64;
        case DXGI_FORMAT_R32G32B32_FLOAT: case DXGI_FORMAT_R32G32B32_UINT: case DXGI_FORMAT_R32G32B32_SINT: return 96;
        case DXGI_FORMAT_R32G32_FLOAT: case DXGI_FORMAT_R32G32_UINT: case DXGI_FORMAT_R32G32_SINT: return 64;
        case DXGI_FORMAT_R32G32B32A32_FLOAT: case DXGI_FORMAT_R32G32B32A32_UINT: case DXGI_FORMAT_R32G32B32A32_SINT: return 128;
        default: return 0;
        }
        }(f);
    if (!bpp) return false;

    rowPitch = (w * bpp) / 8;
    slicePitch = rowPitch * h;
    return true;
}
