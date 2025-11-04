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
using namespace DirectX;

 inline Microsoft::WRL::ComPtr<ID3D11Buffer> g_cb1;

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


// Max instances you plan to send per draw (<= 63 to stay within 4KB)
constexpr uint32_t MAX_INST = 1024;

struct CB1Payload {
	// cb1[0]
	DirectX::XMFLOAT4 meshOffset_meshScale;   // xyz, w
	// cb1[1]
	DirectX::XMFLOAT4 uvScale_uvOffset;       // x, y, z, w(as float)
	// cb1[2..] rows for instances
	DirectX::XMFLOAT4 instances[MAX_INST][4];
};


static void UpdateCB1_Single(
    ID3D11DeviceContext* ctx,
    const glm::vec3& model_offset,
    float             model_scale,
    float             texScale,
    float             texOffX, float texOffY,
    const glm::quat& rot,      // GLM stores (w,x,y,z) but .x/.y/.z/.w are available
    const glm::vec3& pos)
{
    using namespace DirectX;

    CB1Payload_override cb{};

    // --- Build transform (row-major) ---
    // 1) GLM quat -> DirectX: XM expects (x,y,z,w)
    const XMVECTOR q = XMVectorSet(rot.w, rot.x, rot.y, rot.z);
    const XMMATRIX R = XMMatrixRotationQuaternion(q);
    const XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

    // Row-vector convention: apply Basis, then Rotate, then Translate
    const XMMATRIX M = R * T;
    XMStoreFloat4x4(&cb.mesh_to_world, M);

    // --- Other cb1 fields exactly as your VS expects ---
    cb.position_scale = XMFLOAT4(model_scale, model_scale, model_scale, 0.0f);
    cb.position_offset = XMFLOAT4(model_offset.x, model_offset.y, model_offset.z, model_scale);
    cb.texcoord0_scale_offset = XMFLOAT4(texScale, texScale, texOffX, texOffY);
    cb.dynamic_sh_ao_values = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f); // matches capture

    D3D11_MAPPED_SUBRESOURCE m{};
    if (SUCCEEDED(ctx->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m))) {
        std::memcpy(m.pData, &cb, sizeof(cb));
        ctx->Unmap(g_cb1.Get(), 0);
    }
}