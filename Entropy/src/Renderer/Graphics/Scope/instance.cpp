#include "instance.h"

inline DirectX::XMMATRIX MakeWorld(const SStaticInstanceTransform& s) {
	using namespace DirectX;
	const XMMATRIX S = XMMatrixScaling(s.scale.x, s.scale.x, s.scale.x);

	// If your quat is (w,x,y,z) -> reorder to (x,y,z,w)
	const XMVECTOR q = XMVectorSet(s.rotation.w, s.rotation.x, s.rotation.y, s.rotation.z);
	const XMMATRIX R = XMMatrixRotationQuaternion(q);

	const XMMATRIX T = XMMatrixTranslation(s.translation.x, s.translation.y, s.translation.z);

	// Row-major world (DirectXMath is row-major)
	return S * R * T;
}

void UpdateCB1(
	ID3D11DeviceContext* ctx,
	const DirectX::XMFLOAT3& meshOffset, float meshScale,
	float uvScaleX, float uvOffX, float uvOffY, std::uint32_t maxColorOrClampBits,
	const std::vector<SStaticInstanceTransform>& worlds
) {
	using namespace DirectX;

	D3D11_MAPPED_SUBRESOURCE m{};
	ctx->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
	auto* cb = reinterpret_cast<CB1Payload*>(m.pData);

	// Header (matches cbl_v0 / cbl_v1)
	cb->meshOffset_meshScale = XMFLOAT4(meshOffset.x, meshOffset.y, meshOffset.z, meshScale);
	cb->uvScale_uvOffset = XMFLOAT4(uvScaleX, uvOffX, uvOffY, asfloat_u32(maxColorOrClampBits));

	// Per-instance 4x float4 rows
	const UINT n = (UINT)std::min<std::size_t>(worlds.size(), MAX_INST);
	for (UINT i = 0; i < n; ++i) {
		XMMATRIX M = MakeWorld(worlds[i]);
		// If shader expects column-major, uncomment:
		M = XMMatrixTranspose(M);

		XMStoreFloat4(&cb->instances[i][0], M.r[0]);
		XMStoreFloat4(&cb->instances[i][1], M.r[1]);
		XMStoreFloat4(&cb->instances[i][2], M.r[2]);
		XMStoreFloat4(&cb->instances[i][3], M.r[3]);
	}

	// Zero the rest (optional, but keeps the debugger view clean)
	for (UINT i = n; i < MAX_INST; ++i) {
		cb->instances[i][0] = XMFLOAT4(1, 0, 0, 0);
		cb->instances[i][1] = XMFLOAT4(1, 0, 0, 0);
		cb->instances[i][2] = XMFLOAT4(1, 0, 0, 0);
		cb->instances[i][3] = XMFLOAT4(1, 0, 0, 0);
	}

	ctx->Unmap(g_cb1.Get(), 0);

	ID3D11Buffer* b = g_cb1.Get();
	ctx->VSSetConstantBuffers(1, 1, &b);
}
