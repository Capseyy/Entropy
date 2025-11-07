#pragma once
#include <cstddef>
#include <DirectXMath.h>
#include "d3d11.h"
#include <vector>
#include "TigerEngine/Map/static.h"
#include "wrl/client.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>  // translate
#include "Renderer/Tools/ErrorLogger.h"

using namespace DirectX;

 inline Microsoft::WRL::ComPtr<ID3D11Buffer> g_cb1;
 inline Microsoft::WRL::ComPtr<ID3D11Buffer> g_cb1_fallback;

static inline float asfloat_u32(std::uint32_t u) {
	return std::bit_cast<float>(u);
}

void UpdateCB1(
	ID3D11DeviceContext* ctx,
	const DirectX::XMFLOAT3& meshOffset, float meshScale,
	float uvScaleX, float uvOffX, float uvOffY, std::uint32_t maxColorOrClampBits,
	const std::vector<SStaticInstanceTransform>& worlds
);


struct alignas(16) CB1Payload_override
{
	DirectX::XMFLOAT4X4 mesh_to_world;          // row_major, 64 bytes @ 0
	DirectX::XMFLOAT4   position_scale;         // @ 64
	DirectX::XMFLOAT4   position_offset;        // @ 80
	DirectX::XMFLOAT4   texcoord0_scale_offset; // @ 96
	DirectX::XMFLOAT4   dynamic_sh_ao_values;   // @ 112
};


struct CB1Payload {
	// cb1[0]
	DirectX::XMFLOAT4 meshOffset_meshScale;   // xyz, w
	// cb1[1]
	DirectX::XMFLOAT4 uvScale_uvOffset;       // x, y, z, w(as float)
	// cb1[2..] rows for instances
    std::vector<DirectX::XMFLOAT4> instances[4];
};


static void UpdateCB1_Single(
    ID3D11DeviceContext* ctx,
    glm::vec4 model_offset,
    glm::vec4             model_scale,
    float            instance_scale,
    float             texScale,
    float             texOffX, float texOffY,
    const glm::quat& rot,      // GLM stores (w,x,y,z) but .x/.y/.z/.w are available
    const glm::vec3& pos)
{
    using namespace DirectX;

    CB1Payload_override cb{};

    const XMVECTOR q = XMVectorSet(rot.w, rot.x, rot.y, rot.z);
    const XMMATRIX R = XMMatrixRotationQuaternion(q);
    const XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // Uniform scale goes IN THE MATRIX:
    float s = instance_scale;
    const XMMATRIX S = XMMatrixScaling(s, s, s);

    // Row-vector convention: Scale ? Rotate ? Translate
    const XMMATRIX M = S * R * T;
    XMStoreFloat4x4(&cb.mesh_to_world, M);

    // Keep authored per-mesh scale in cb (unchanged) to avoid double scaling.
    cb.position_scale = XMFLOAT4(model_scale.x, model_scale.y, model_scale.z, model_scale.w);
    cb.position_offset = XMFLOAT4(model_offset.x, model_offset.y, model_offset.z, model_offset.w);

    cb.texcoord0_scale_offset = XMFLOAT4(texScale, texScale, texOffX, texOffY);
    cb.dynamic_sh_ao_values = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        std::memcpy(m.pData, &cb, sizeof(cb));
        ctx->Unmap(g_cb1.Get(), 0);
    }
}

inline DirectX::XMMATRIX MakeWorld(const ObjectVectors& s) {
    using namespace DirectX;
    const XMMATRIX S = XMMatrixScaling(s.scale, s.scale, s.scale);

    // If your quat is (w,x,y,z) -> reorder to (x,y,z,w)
    const XMVECTOR q = XMVectorSet(s.rotation.w, s.rotation.x, s.rotation.y, s.rotation.z);
    const XMMATRIX R = XMMatrixRotationQuaternion(q);

    const XMMATRIX T = XMMatrixTranslation(s.translation.x, s.translation.y, s.translation.z);

    // Row-major world (DirectXMath is row-major)
    return S * R * T;
}

inline float from_bytes(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    uint32_t u = (uint32_t)b0 | ((uint32_t)b1 << 8) | ((uint32_t)b2 << 16) | ((uint32_t)b3 << 24);
    float f;
    std::memcpy(&f, &u, sizeof(f));
    return f;
}

static void CreateCB1_FreshDynamic(
    ID3D11Device* device,
    ID3D11DeviceContext* ctx,
    const DirectX::XMFLOAT3& meshOffset, float meshScale,
    float uvScaleX, float uvOffX, float uvOffY, std::uint32_t maxColorOrClampBits,
    const std::vector<ObjectVectors>& worlds, Microsoft::WRL::ComPtr<ID3D11Buffer>& b)
{
    using namespace DirectX;

    // 64KB constant buffer cap -> max instances = (65536 - 32) / 64 = 1023
    const size_t instCap = (65536u - 32u) / 64u;
    const size_t instCount = std::min(worlds.size(), instCap);

    // EXACT row count: 2 header + 4 per instance
    const size_t rowCount = 2 + instCount * 4;
    std::vector<XMFLOAT4> rows(rowCount);   // <-- sized, no push_back/emplace_back later

    // Header
    rows[0] = XMFLOAT4(meshOffset.x, meshOffset.y, meshOffset.z, meshScale);
    rows[1] = XMFLOAT4(uvScaleX, uvOffX, uvOffY, asfloat_u32(maxColorOrClampBits));

    // Instances: write by INDEX (no accidental extras)
    XMFLOAT4 xm4;
    xm4.w = 1.0f;
    xm4.x = 1.0f;
    xm4.y = 1.0f;
    xm4.z = 9.40395e-38;


    for (size_t i = 0; i < instCount; ++i)
    {
        const size_t base = 2 + i * 4;

        XMMATRIX M = MakeWorld(worlds[i]);
        XMMATRIX Mt = XMMatrixTranspose(M); // HLSL column-major readability

        float kMagic = from_bytes(0xF7, 0xFF, 0xFF, 0x01);
        Mt.r[3] = DirectX::XMVectorSet(1.0f, 1.0f, 1.0f, kMagic);
        XMStoreFloat4(&rows[base + 0], Mt.r[0]);
        XMStoreFloat4(&rows[base + 1], Mt.r[1]);
        XMStoreFloat4(&rows[base + 2], Mt.r[2]);
        XMStoreFloat4(&rows[base + 3], Mt.r[3]);
    }

    const UINT byteSize = static_cast<UINT>(rows.size() * sizeof(XMFLOAT4)); // 16 * rowCount

    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = byteSize;
    bd.Usage = D3D11_USAGE_IMMUTABLE; // fresh each call
    bd.CPUAccessFlags = 0;

    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rows.data();

    b.Reset();
    HRESULT hr = device->CreateBuffer(&bd, &init, b.GetAddressOf());
}


