#pragma once
#include <cstddef>
#include <DirectXMath.h>
#include "d3d11.h"
#include <vector>
#include "TigerEngine/Map/static.h"
#include "wrl/client.h"
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
