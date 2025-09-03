#pragma once
#include <windows.h>
#include <d3d11.h>
#include <DirectXMath.h>
#include "TigerEngine/tag.h"



#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

enum class DxgiFormat : std::uint32_t {
    Unknown = 0,
    R32G32B32A32_TYPELESS = 1,
    R32G32B32A32_FLOAT = 2,
    R32G32B32A32_UINT = 3,
    R32G32B32A32_SINT = 4,
    R32G32B32_TYPELESS = 5,
    R32G32B32_FLOAT = 6,
    R32G32B32_UINT = 7,
    R32G32B32_SINT = 8,
    R16G16B16A16_TYPELESS = 9,
    R16G16B16A16_FLOAT = 10,
    R16G16B16A16_UNORM = 11,
    R16G16B16A16_UINT = 12,
    R16G16B16A16_SNORM = 13,
    R16G16B16A16_SINT = 14,
    R32G32_TYPELESS = 15,
    R32G32_FLOAT = 16,
    R32G32_UINT = 17,
    R32G32_SINT = 18,
    R32G8X24_TYPELESS = 19,
    D32_FLOAT_S8X24_UINT = 20,
    R32_FLOAT_X8X24_TYPELESS = 21,
    X32_TYPELESS_G8X24_UINT = 22,
    R10G10B10A2_TYPELESS = 23,
    R10G10B10A2_UNORM = 24,
    R10G10B10A2_UINT = 25,
    R11G11B10_FLOAT = 26,
    R8G8B8A8_TYPELESS = 27,
    R8G8B8A8_UNORM = 28,
    R8G8B8A8_UNORM_SRGB = 29,
    R8G8B8A8_UINT = 30,
    R8G8B8A8_SNORM = 31,
    R8G8B8A8_SINT = 32,
    R16G16_TYPELESS = 33,
    R16G16_FLOAT = 34,
    R16G16_UNORM = 35,
    R16G16_UINT = 36,
    R16G16_SNORM = 37,
    R16G16_SINT = 38,
    R32_TYPELESS = 39,
    D32_FLOAT = 40,
    R32_FLOAT = 41,
    R32_UINT = 42,
    R32_SINT = 43,
    R24G8_TYPELESS = 44,
    D24_UNORM_S8_UINT = 45,
    R24_UNORM_X8_TYPELESS = 46,
    X24_TYPELESS_G8_UINT = 47,
    R8G8_TYPELESS = 48,
    R8G8_UNORM = 49,
    R8G8_UINT = 50,
    R8G8_SNORM = 51,
    R8G8_SINT = 52,
    R16_TYPELESS = 53,
    R16_FLOAT = 54,
    D16_UNORM = 55,
    R16_UNORM = 56,
    R16_UINT = 57,
    R16_SNORM = 58,
    R16_SINT = 59,
    R8_TYPELESS = 60,
    R8_UNORM = 61,
    R8_UINT = 62,
    R8_SNORM = 63,
    R8_SINT = 64,
    A8_UNORM = 65,
    R1_UNORM = 66,
    R9G9B9E5_SHAREDEXP = 67,
    R8G8_B8G8_UNORM = 68,
    G8R8_G8B8_UNORM = 69,
    BC1_TYPELESS = 70,
    BC1_UNORM = 71,
    BC1_UNORM_SRGB = 72,
    BC2_TYPELESS = 73,
    BC2_UNORM = 74,
    BC2_UNORM_SRGB = 75,
    BC3_TYPELESS = 76,
    BC3_UNORM = 77,
    BC3_UNORM_SRGB = 78,
    BC4_TYPELESS = 79,
    BC4_UNORM = 80,
    BC4_SNORM = 81,
    BC5_TYPELESS = 82,
    BC5_UNORM = 83,
    BC5_SNORM = 84,
    B5G6R5_UNORM = 85,
    B5G5R5A1_UNORM = 86,
    B8G8R8A8_UNORM = 87,
    B8G8R8X8_UNORM = 88,
    R10G10B10_XR_BIAS_A2_UNORM = 89,
    B8G8R8A8_TYPELESS = 90,
    B8G8R8A8_UNORM_SRGB = 91,
    B8G8R8X8_TYPELESS = 92,
    B8G8R8X8_UNORM_SRGB = 93,
    BC6H_TYPELESS = 94,
    BC6H_UF16 = 95,
    BC6H_SF16 = 96,
    BC7_TYPELESS = 97,
    BC7_UNORM = 98,
    BC7_UNORM_SRGB = 99,
    AYUV = 100,
    Y410 = 101,
    Y416 = 102,
    NV12 = 103,
    P010 = 104,
    P016 = 105,
    OPAQUE420 = 106,
    YUY2 = 107,
    Y210 = 108,
    Y216 = 109,
    NV11 = 110,
    AI44 = 111,
    IA44 = 112,
    P8 = 113,
    A8P8 = 114,
    B4G4R4A4_UNORM = 115,
    P208 = 130,
    V208 = 131,
    V408 = 132,
    SAMPLER_FEEDBACK_MIN_MIP_OPAQUE,
    SAMPLER_FEEDBACK_MIP_REGION_USED_OPAQUE,
    FORCE_UINT = 0xffffffff,
};




struct VertexP
{
    DirectX::XMFLOAT3 position; // 3D position
};

struct VertexPNUTC
{
    DirectX::XMFLOAT4 position; // 0  .. 11
    DirectX::XMFLOAT3 normal;   // 12 .. 23
    DirectX::XMFLOAT2 uv;       // 24 .. 31
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT4 tangent;

    VertexPNUTC() :
        position(0, 0, 0, 0), normal(0, 0, 0), uv(0, 0), color(1, 1, 1, 1) {
    }

    VertexPNUTC(const DirectX::XMFLOAT4& p,
        const DirectX::XMFLOAT3& n,
        const DirectX::XMFLOAT2& t,
        const DirectX::XMFLOAT4& c,
        const DirectX::XMFLOAT4& tan)
        : position(p), normal(n), uv(t), color(c), tangent(tan) {
    }
};

// Sanity checks (will fail at compile-time if layout changes)
static_assert(sizeof(VertexPNUTC) == 68, "VertexPNUTC must be 68 bytes");

// D3D11 input layout for the struct above
static const D3D11_INPUT_ELEMENT_DESC g_InputElements_PNUTC[] =
{
    // SemanticName, SemanticIndex, Format, InputSlot, AlignedByteOffset, InputSlotClass, InstanceDataStepRate
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 0,  D3D11_INPUT_PER_VERTEX_DATA, 0 },   // position (float3)
    { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT,    0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },   // normal   (float3)
    { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,       0, 24, D3D11_INPUT_PER_VERTEX_DATA, 0 },   // uv       (float2)
    { "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D11_INPUT_PER_VERTEX_DATA, 0 },   // color    (float4)
};
// total stride:
static const UINT g_Stride_PNUTC = sizeof(VertexPNUTC);

struct VertexData {
    std::vector<DirectX::XMFLOAT4> positions;
	std::vector<DirectX::XMFLOAT2> uvs;
	std::vector<DirectX::XMFLOAT4> colors;
	std::vector<DirectX::XMFLOAT3> normals;
    std::vector<DirectX::XMFLOAT4> tangents;
};

struct StaticRenderPart {
    std::vector<uint32_t> cubeIdx;
    uint8_t TfxRenderStage;
    TagHash technique;
    uint8_t input_layout_index;
    UINT faceCount;
    UINT buffer_index;
    UINT LodCatagory;
};

// --- pretty print ---
inline std::ostream& operator<<(std::ostream& os, const StaticRenderPart& p)
{
    os << "StaticRenderPart {\n";
    os << "  cubeIdx.size   = " << p.cubeIdx.size() << "\n";
    os << "  TfxRenderStage = " << static_cast<unsigned>(p.TfxRenderStage) << "\n";
    os << "  technique.hash = 0x" << std::hex << p.technique.hash << std::dec << "\n";
    os << "  input_layout   = " << static_cast<unsigned>(p.input_layout_index) << "\n";
    os << "  faceCount      = " << p.faceCount << "\n";
    os << "  buffer_index   = " << p.buffer_index << "\n";
    os << "  LodCatagory    = " << p.LodCatagory << "\n";
    os << "}";
    return os;
}



struct VertexBuffer {
    ID3D11Buffer* buffer;
    uint32_t size;
    uint32_t length;
    uint32_t stride;
};

struct IndexBuffer {
    ID3D11Buffer* buffer;
    size_t size;
    DxgiFormat format;
};

struct IndexHeader {
    uint8_t Unk0;
    uint8_t is_32_bit;
    uint16_t unk02;
    uint32_t zero;
	uint64_t size;
	uint32_t deadbeef;
    uint32_t zero1;

    std::vector<uint32_t> process_buffer(uint8_t primitive_type, uint32_t index_start, uint32_t index_count, uint32_t hash);
};

struct VertexHeader {
    uint32_t size;
    uint16_t stride;
    uint16_t type;
    uint32_t deadbeef;

    VertexData process_buffer(uint32_t hash, std::array<float_t, 3> mesh_offset, float_t mesh_scale, float_t texture_coordinate_scale, std::array<float_t, 2> texture_coordinate_offset);
};


struct StaticRenderObject {

    std::vector<std::vector<VertexPNUTC>> buffers;
    DirectX::XMFLOAT3 transform;
    DirectX::XMFLOAT3 rotation;
    std::vector<StaticRenderPart> parts;
};

std::vector<VertexPNUTC> BuildVertices(const std::vector<DirectX::XMFLOAT4>& positions,
    const std::vector<DirectX::XMFLOAT3>& normals,
    const std::vector<DirectX::XMFLOAT2>& uvs,
    const std::vector<DirectX::XMFLOAT4>& colors,
    const std::vector<DirectX::XMFLOAT4>& tangents);


