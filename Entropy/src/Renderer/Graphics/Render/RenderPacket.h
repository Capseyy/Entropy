
#include <d3d11.h>
#include <cstdint>
#include "Runtime/Assets/AssetSystem.h"

struct MeshParams {
    DirectX::XMFLOAT3 mesh_offset; float mesh_scale = 1.0f;
    float uv_scale = 1.0f, uv_off_x = 0.0f, uv_off_y = 0.0f, max_colour = 0.0f;
};

enum class DrawPacketType { Static, Entity, Light, Terrian };

static inline float asfloat_u321(std::uint32_t u) {
    float f; std::memcpy(&f, &u, sizeof(u)); return f;
}
static inline float from_bytes1(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3) {
    const std::uint32_t u = (std::uint32_t(b0)) | (std::uint32_t(b1) << 8) |
        (std::uint32_t(b2) << 16) | (std::uint32_t(b3) << 24);
    return asfloat_u321(u);
}

struct CB1Cpu {
    DirectX::XMFLOAT3 mesh_offset;   
    float             mesh_scale;    

    float             uv_scale;      
    float             uv_off_x;      
    float             uv_off_y;      
    std::uint32_t     flags_or_maxColorBits;
};


struct CB1HeaderGPU {
    DirectX::XMFLOAT4 meshOffset_meshScale;
    DirectX::XMFLOAT4 uvScale_uvOffset;  
};
struct CB1InstanceRowGPU {
    DirectX::XMFLOAT4 row0;
    DirectX::XMFLOAT4 row1;
    DirectX::XMFLOAT4 row2;
    DirectX::XMFLOAT4 row3;
};

static constexpr UINT kCB1_CAP_BYTES = 64 * 1024;
static constexpr UINT kCB1_HDR_BYTES = sizeof(CB1HeaderGPU);      
static constexpr UINT kCB1_ROW_BYTES = sizeof(CB1InstanceRowGPU); 
static constexpr UINT kCB1_MAX_INST = (kCB1_CAP_BYTES - kCB1_HDR_BYTES) / kCB1_ROW_BYTES; 


static void EnsureCB1_StaticReusable(ID3D11Device* device,
    Microsoft::WRL::ComPtr<ID3D11Buffer>& b)
{
    if (b) return;

    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = kCB1_CAP_BYTES;
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    HRESULT hr = device->CreateBuffer(&bd, nullptr, b.GetAddressOf());
   
}


static void UpdateCB1_StaticReusable(
    ID3D11DeviceContext* ctx,
    const MeshParams& hdr,          
    const ObjectVectors* worlds,   
    uint32_t worldCount,
    ID3D11Buffer* cb1)
{
    using namespace DirectX;


    constexpr UINT kCB1_CAP_BYTES = 64u * 1024u;
    constexpr UINT kHDR_BYTES = 32u; 
    constexpr UINT kROW_BYTES = 16u; 
    constexpr UINT kINST_BYTES = 4u * kROW_BYTES;
    const     UINT kINST_CAP = (kCB1_CAP_BYTES - kHDR_BYTES) / kINST_BYTES;

    const UINT instCount = (UINT)std::min<std::size_t>(worldCount, kINST_CAP);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (FAILED(ctx->Map(cb1, 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
        return;

    auto* rows = reinterpret_cast<XMFLOAT4*>(m.pData);

    
    rows[0] = XMFLOAT4(hdr.mesh_offset.x, hdr.mesh_offset.y, hdr.mesh_offset.z, hdr.mesh_scale);
    rows[1] = XMFLOAT4(hdr.uv_scale, hdr.uv_off_x, hdr.uv_off_y, asfloat_u32(hdr.max_colour));

    
    
    const float kMagic = asfloat_u32(0x01FFFFF7u);

    if (instCount && worlds) {
        for (UINT i = 0; i < instCount; ++i) {
            const UINT base = 2 + i * 4;

            
            const XMMATRIX M = MakeWorld(worlds[i]);   
            const XMMATRIX Mt = XMMatrixTranspose(M);   

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
  
    const float x = ov.translation.x, y = ov.translation.y, z = ov.translation.z, w = 1.0f;
    const float* m = &world_to_camera.m[0][0]; 
    const float vz = m[2] * x + m[6] * y + m[10] * z + m[14] * w;
   
    const float nz = std::min(std::max(vz, -1e6f), 1e6f);
    const float k = (nz + 1e6f) * (1.0f / 2e6f); 
    return (uint32_t)(k * 0xFFFFFFu);
}

struct DrawPacket {

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

   
    Microsoft::WRL::ComPtr<ID3D11Buffer> instancesCB; 
    UINT                      instanceCount = 0;
    MeshParams cb1;

    uint32_t                  sortKeyLow = 0;
    UINT baseInstance = 0;  
    std::uint32_t     flags_or_maxColorBits; 
    uint32_t worldOffset = 0;
    uint32_t worldCount = 0;
};


static inline uint64_t MakeStateKey(const DrawPacket& p) {
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
    out.flags_or_maxColorBits = mp.max_colour; 
    return out;
}
