#include <cstdint>
#include <vector>
#include <algorithm>
#include <cstring>
#include "TigerEngine/Technique/technique.h"
#undef max
#undef min

// ---- you already have this in your project; shown only for fields we use ----
// struct STextureHeader { uint32_t dxgiFormat; uint16_t width, height, depth, arraySize; uint8_t mipCount; };

// ----------------------- format info helpers -----------------------
struct FormatInfo { bool blockCompressed; uint32_t bpp; uint32_t blockBytes; };
static inline FormatInfo GetFormatInfo(uint32_t dxgi)
{
    switch (dxgi)
    {
        // RGBA8
    case 28: case 87: return { false, 32, 0 };
           // BC1/BC4 = 8 bytes per 4x4 block
    case 71: case 72: case 80: case 81: return { true, 0, 8 };
           // BC2/BC3/BC5/BC6/BC7 = 16 bytes per 4x4 block
    case 74: case 75: case 77: case 78: case 83: case 84: case 95: case 96: case 98: case 99:
        return { true, 0, 16 };
    default: return { false, 0, 0 };
    }
}

static inline size_t MipLevelSizeBytes(uint32_t w, uint32_t h, uint32_t d, const FormatInfo& fi)
{
    w = std::max(1u, w); h = std::max(1u, h); d = std::max(1u, d);
    if (fi.blockCompressed) {
        uint32_t bw = (w + 3) / 4, bh = (h + 3) / 4;
        return size_t(bw) * size_t(bh) * size_t(fi.blockBytes) * size_t(d);
    }
    else if (fi.bpp) {
        uint64_t row = (uint64_t(w) * fi.bpp + 7u) / 8u;
        return size_t(row) * size_t(h) * size_t(d);
    }
    return 0;
}

struct MipFit { uint32_t count; size_t totalBytes; };
static inline MipFit ComputeMipCountThatFits(uint32_t w, uint32_t h, uint32_t d,
    uint32_t declaredMips, const FormatInfo& fi,
    size_t payloadSize)
{
    uint32_t mw = std::max(1u, w), mh = std::max(1u, h), md = std::max(1u, d);
    uint32_t used = 0; size_t acc = 0;
    for (; used < std::max(1u, declaredMips); ++used) {
        size_t sz = MipLevelSizeBytes(mw, mh, md, fi);
        if (acc + sz > payloadSize) break;
        acc += sz;
        mw = std::max(1u, mw >> 1);
        mh = std::max(1u, mh >> 1);
        if (md > 1) md >>= 1;
    }
    return { used, acc };
}

// ----------------------- diagnostics (optional) -----------------------
struct DDSDiag {
    uint32_t dxgiFormat, width, height, depth, arraySize, mipCount;
    bool isBC; uint32_t bpp, blockBytes; size_t payloadSize, expectedSize;
};
static inline DDSDiag ComputeDDSDiag(const STextureHeader& hdr, size_t payloadSize)
{
    FormatInfo fi = GetFormatInfo(hdr.dxgiFormat);
    uint32_t w = std::max<uint32_t>(1, hdr.width);
    uint32_t h = std::max<uint32_t>(1, hdr.height);
    uint32_t d = std::max<uint32_t>(1, hdr.depth);
    uint32_t a = std::max<uint32_t>(1, hdr.arraySize);
    uint32_t m = std::max<uint32_t>(1, hdr.mipCount);

    size_t expected = 0;
    for (uint32_t s = 0; s < a; ++s) {
        uint32_t mw = w, mh = h, md = d;
        for (uint32_t mi = 0; mi < m; ++mi) {
            expected += MipLevelSizeBytes(mw, mh, md, fi);
            mw = std::max(1u, mw >> 1);
            mh = std::max(1u, mh >> 1);
            if (md > 1) md >>= 1;
        }
    }
    return { hdr.dxgiFormat, w, h, d, a, m, fi.blockCompressed, fi.bpp, fi.blockBytes, payloadSize, expected };
}

// ----------------------- DDS structs (no SDK name conflicts) -----------------------
static const uint32_t kDDS_MAGIC = 0x20534444; // "DDS "

// header flags
static const uint32_t kDDSD_CAPS = 0x00000001;
static const uint32_t kDDSD_HEIGHT = 0x00000002;
static const uint32_t kDDSD_WIDTH = 0x00000004;
static const uint32_t kDDSD_PITCH = 0x00000008;
static const uint32_t kDDSD_PIXELFORMAT = 0x00001000;
static const uint32_t kDDSD_MIPMAPCOUNT = 0x00020000;
static const uint32_t kDDSD_LINEARSIZE = 0x00080000;
static const uint32_t kDDSD_DEPTH = 0x00800000;

// caps
static const uint32_t kDDSCAPS_COMPLEX = 0x00000008;
static const uint32_t kDDSCAPS_TEXTURE = 0x00001000;
static const uint32_t kDDSCAPS_MIPMAP = 0x00400000;

// caps2
static const uint32_t kDDSCAPS2_CUBEMAP = 0x00000200;
static const uint32_t kDDSCAPS2_VOLUME = 0x00200000;

// pixelformat
static const uint32_t kDDPF_FOURCC = 0x00000004;

// DX10 chunk
static const uint32_t kFOURCC_DX10 = 0x30315844; // "DX10"

// resource dimension (avoid D3D enum names)
enum DDS_RES_DIM : uint32_t { DDS_RES_DIM_TEX1D = 2, DDS_RES_DIM_TEX2D = 3, DDS_RES_DIM_TEX3D = 4 };
static const uint32_t kDDS_MISC_TEXTURECUBE = 0x4;

#pragma pack(push,1)
struct DDS_PIXELFMT { uint32_t size, flags, fourCC, RGBBitCount, RBitMask, GBitMask, BBitMask, ABitMask; };
struct DDS_HEADER {
    uint32_t size, flags, height, width, pitchOrLinearSize, depth, mipMapCount;
    uint32_t reserved1[11];
    DDS_PIXELFMT ddspf;
    uint32_t caps, caps2, caps3, caps4, reserved2;
};
struct DDS_HEADER_DX10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};
#pragma pack(pop)

// ----------------------- DDS builder -----------------------
static inline std::vector<uint8_t> BuildDDSFromTigerHeader(
    const STextureHeader& H, const uint8_t* payload, size_t payloadSize, bool forceCube = false)
{
    std::vector<uint8_t> out;
    if (!payload || payloadSize == 0) return out;

    FormatInfo fi = GetFormatInfo(H.dxgiFormat);
    if (!fi.blockCompressed && fi.bpp == 0) {
        // Unknown format – assume BC7 sizing so we can at least try
        fi = { true, 0, 16 };
    }

    const uint32_t width = std::max<uint32_t>(1, H.width);
    const uint32_t height = std::max<uint32_t>(1, H.height);
    uint32_t       depth = std::max<uint32_t>(1, H.depth);
    const uint32_t arrayCount = std::max<uint32_t>(1, H.arraySize);
    uint32_t       declaredMips = std::max<uint32_t>(1, H.mipCount);

    // prefer 2D array over 3D unless truly volume
    if (arrayCount > 1 && depth > 1) depth = 1;

    // clamp mips to what fits in payload
    MipFit fit = ComputeMipCountThatFits(width, height, depth, declaredMips, fi, payloadSize);
    if (fit.count == 0) return out;

    const bool isCube = forceCube && (arrayCount % 6 == 0) && (width == height) && (depth == 1);

    // build headers
    DDS_HEADER hdr{}; hdr.size = sizeof(DDS_HEADER);
    hdr.flags = kDDSD_CAPS | kDDSD_HEIGHT | kDDSD_WIDTH | kDDSD_PIXELFORMAT;
    hdr.width = width; hdr.height = height;
    hdr.depth = (depth > 1) ? depth : 0;
    hdr.mipMapCount = fit.count;
    if (fit.count > 1) hdr.flags |= kDDSD_MIPMAPCOUNT;
    if (depth > 1)     hdr.flags |= kDDSD_DEPTH;

    // top-level pitch/linear size
    if (fi.blockCompressed) {
        uint32_t bw = (width + 3) / 4, bh = (height + 3) / 4;
        hdr.pitchOrLinearSize = bw * bh * fi.blockBytes;
        hdr.flags |= kDDSD_LINEARSIZE;
    }
    else {
        uint32_t row = (width * fi.bpp + 7) / 8;
        hdr.pitchOrLinearSize = row;
        hdr.flags |= kDDSD_PITCH;
    }

    hdr.ddspf.size = sizeof(DDS_PIXELFMT);
    hdr.ddspf.flags = kDDPF_FOURCC;
    hdr.ddspf.fourCC = kFOURCC_DX10;
    hdr.caps = kDDSCAPS_TEXTURE | ((fit.count > 1) ? (kDDSCAPS_COMPLEX | kDDSCAPS_MIPMAP) : 0);
    hdr.caps2 = (depth > 1) ? kDDSCAPS2_VOLUME : (isCube ? kDDSCAPS2_CUBEMAP : 0);

    DDS_HEADER_DX10 dx10{};
    dx10.dxgiFormat = H.dxgiFormat;
    dx10.resourceDimension = (depth > 1) ? DDS_RES_DIM_TEX3D : (height == 1 ? DDS_RES_DIM_TEX1D : DDS_RES_DIM_TEX2D);
    dx10.miscFlag = isCube ? kDDS_MISC_TEXTURECUBE : 0;
    dx10.arraySize = arrayCount ? arrayCount : 1;
    dx10.miscFlags2 = 0;

    // write DDS
    out.reserve(4 + sizeof(hdr) + sizeof(dx10) + fit.totalBytes);
    const uint32_t magic = kDDS_MAGIC;
    out.insert(out.end(), (const uint8_t*)&magic, (const uint8_t*)&magic + 4);
    out.insert(out.end(), (const uint8_t*)&hdr, (const uint8_t*)&hdr + sizeof(hdr));
    out.insert(out.end(), (const uint8_t*)&dx10, (const uint8_t*)&dx10 + sizeof(dx10));
    out.insert(out.end(), payload, payload + fit.totalBytes);

    return out;
}