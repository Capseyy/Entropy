// Graphics.h (or a small RenderPackets.h)
#include <d3d11.h>
#include <cstdint>
#include "Runtime/Assets/AssetSystem.h"

struct MeshParams {
    DirectX::XMFLOAT3 mesh_offset; float mesh_scale = 1.0f;
    float uv_scale = 1.0f, uv_off_x = 0.0f, uv_off_y = 0.0f, max_colour = 0.0f;
};

static inline float asfloat_u321(std::uint32_t u) {
    float f; std::memcpy(&f, &u, sizeof(u)); return f;
}
static inline float from_bytes1(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    const std::uint32_t u = (std::uint32_t(b0)) | (std::uint32_t(b1) << 8) |
        (std::uint32_t(b2) << 16) | (std::uint32_t(b3) << 24);
    return asfloat_u321(u);
}

struct CB1Cpu {
    DirectX::XMFLOAT3 mesh_offset;   // xyz
    float             mesh_scale;    // w of cb1[0]

    float             uv_scale;      // x of cb1[1]
    float             uv_off_x;      // y of cb1[1]
    float             uv_off_y;      // z of cb1[1]
    std::uint32_t     flags_or_maxColorBits; // packed bits, sent as float in cb1[1].w
};

// GPU layout: 32B header + N * 64B rows (4 x float4 per instance)
struct CB1HeaderGPU {
    DirectX::XMFLOAT4 meshOffset_meshScale; // xyz, w
    DirectX::XMFLOAT4 uvScale_uvOffset;     // x=scale, y=offX, z=offY, w=bits-as-float
};
struct CB1InstanceRowGPU {
    DirectX::XMFLOAT4 row0;
    DirectX::XMFLOAT4 row1;
    DirectX::XMFLOAT4 row2;
    DirectX::XMFLOAT4 row3;
};

static constexpr UINT kCB1_CAP_BYTES = 64 * 1024;
static constexpr UINT kCB1_HDR_BYTES = sizeof(CB1HeaderGPU);      // 32
static constexpr UINT kCB1_ROW_BYTES = sizeof(CB1InstanceRowGPU); // 64
static constexpr UINT kCB1_MAX_INST = (kCB1_CAP_BYTES - kCB1_HDR_BYTES) / kCB1_ROW_BYTES; // 1023


static void EnsureCB1_StaticReusable(ID3D11Device* device,
    Microsoft::WRL::ComPtr<ID3D11Buffer>& b)
{
    if (b) return;

    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = kCB1_CAP_BYTES;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    // IMPORTANT: no initial data for a dynamic CB
    HRESULT hr = device->CreateBuffer(&bd, nullptr, b.GetAddressOf());
    // handle hr if you want
}


static void UpdateCB1_StaticReusable(
    ID3D11DeviceContext* ctx,
    const MeshParams& hdr,           // meshOffset/Scale + UV + max_colour bits
    const ObjectVectors* worlds,     // pointer into your frameWorlds_ arena (can be null if count==0)
    uint32_t worldCount,
    ID3D11Buffer* cb1)
{
    using namespace DirectX;

    // 64KB constant buffer cap -> max instances = (65536 - 32) / 64 = 1023
    constexpr UINT kCB1_CAP_BYTES = 64u * 1024u;
    constexpr UINT kHDR_BYTES = 32u; // 2 * float4
    constexpr UINT kROW_BYTES = 16u; // float4
    constexpr UINT kINST_BYTES = 4u * kROW_BYTES;
    const     UINT kINST_CAP = (kCB1_CAP_BYTES - kHDR_BYTES) / kINST_BYTES;

    const UINT instCount = (UINT)std::min<std::size_t>(worldCount, kINST_CAP);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx->Map(cb1, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
        return;

    auto* rows = reinterpret_cast<XMFLOAT4*>(m.pData);

    // ---- header (2 rows) ----
    rows[0] = XMFLOAT4(hdr.mesh_offset.x, hdr.mesh_offset.y, hdr.mesh_offset.z, hdr.mesh_scale);
    rows[1] = XMFLOAT4(hdr.uv_scale, hdr.uv_off_x, hdr.uv_off_y, asfloat_u32(hdr.max_colour));

    // ---- instances (4 rows each) ----
    // little-endian byte pattern F7 FF FF 01 (what you used before)
    const float kMagic = asfloat_u32(0x01FFFFF7u);

    if (instCount && worlds) {
        for (UINT i = 0; i < instCount; ++i) {
            const UINT base = 2 + i * 4;

            // Build world matrix from your ObjectVectors
            const XMMATRIX M = MakeWorld(worlds[i]);   // row-major
            const XMMATRIX Mt = XMMatrixTranspose(M);   // HLSL-friendly (column-major)

            XMStoreFloat4(&rows[base + 0], Mt.r[0]);
            XMStoreFloat4(&rows[base + 1], Mt.r[1]);
            XMStoreFloat4(&rows[base + 2], Mt.r[2]);

            XMFLOAT4 r3; XMStoreFloat4(&r3, Mt.r[3]);
            r3.w = kMagic;
            rows[base + 3] = r3;
        }
    }

    ctx->Unmap(cb1, 0);
}

static inline uint32_t EncodeDepthKey(const XMFLOAT4X4& world_to_camera, const ObjectVectors& ov)
{
    // view-space Z from instance translation
    const float x = ov.translation.x, y = ov.translation.y, z = ov.translation.z, w = 1.0f;
    const float* m = &world_to_camera.m[0][0]; // row-major
    const float vz = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
    // larger (farther) should sort first => map to unsigned with clamp
    const float nz = std::min(std::max(vz, -1e6f), 1e6f);
    const float k = (nz + 1e6f) * (1.0f / 2e6f); // 0..1
    return (uint32_t)(k * 0xFFFFFFu);
}

struct DrawPacket {
    // “state” that, if unchanged, lets us skip rebinding:
    uint32_t                  meshId = 0;
    uint8_t                   inputLayoutIndex = 0;
    ID3D11InputLayout* layout = nullptr;

    ID3D11Buffer* vb0 = nullptr;
    ID3D11Buffer* vb1 = nullptr;
    UINT                      stride0 = 0, stride1 = 0;
	ID3D11ShaderResourceView* bVol = nullptr;

    ID3D11Buffer* ib = nullptr;
    DXGI_FORMAT               idxFmt = DXGI_FORMAT_R32_UINT;

    EntropyAssets::Technique* tech = nullptr;
    D3D11_PRIMITIVE_TOPOLOGY  topo = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

    UINT                      indexCount = 0;
    UINT                      firstIndex = 0;
    std::vector<ObjectVectors>* instanceWorlds = nullptr;

    // per-draw varying bits
    Microsoft::WRL::ComPtr<ID3D11Buffer> instancesCB; // your instancing CB (g_cb1 or per-mesh CB)
    UINT                      instanceCount = 0;
    MeshParams cb1;
    // optional: for front-to-back, material, etc.
    uint32_t                  sortKeyLow = 0;
    UINT baseInstance = 0;   // NEW: start index into gInstances
    std::uint32_t     flags_or_maxColorBits; // packed bits, sent as float in cb1[1].w
    uint32_t worldOffset = 0;
    uint32_t worldCount = 0;

};


static inline uint64_t MakeStateKey(const DrawPacket& p) {
    // A cheap, stable grouping key. Collisions are practically harmless.
    return  (uint64_t)(uintptr_t)p.tech ^
        ((uint64_t)(uintptr_t)p.layout << 13) ^
        ((uint64_t)(uintptr_t)p.vb0 << 17) ^
        ((uint64_t)(uintptr_t)p.ib << 21) ^
        ((uint64_t)p.topo << 25);
}

static inline CB1Cpu MakeCB1Cpu(const MeshParams& mp)
{
    CB1Cpu out{};
    out.mesh_offset = DirectX::XMFLOAT3(mp.mesh_offset.x, mp.mesh_offset.y, mp.mesh_offset.z);
    out.mesh_scale = mp.mesh_scale;
    out.uv_scale = mp.uv_scale;
    out.uv_off_x = mp.uv_off_x;
    out.uv_off_y = mp.uv_off_y;
    out.flags_or_maxColorBits = mp.max_colour; // already packed bits
    return out;
}
