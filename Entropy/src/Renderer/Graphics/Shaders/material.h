#pragma once
#include "Shaders.h"
#include <vector>
#include "d3d11.h"
#include "wrl/client.h"



#pragma pack(push, 1)
struct UT_SamplerRaw
{
	uint32_t Filter;          // numeric D3D11_FILTER value in your data
	uint32_t AddressU;        // numeric D3D11_TEXTURE_ADDRESS_MODE
	uint32_t AddressV;
	uint32_t AddressW;
	float    MipLODBias;
	uint32_t MaxAnisotropy;
	uint32_t ComparisonFunc;  // numeric D3D11_COMPARISON_FUNC
	float    BorderColor[4];
	float    MinLOD;
	float    MaxLOD;          // 0 or negative can mean “no clamp”
};
// size should be 52 on MSVC; keep pack(1) so it matches your blob

#pragma pack(pop)

struct Unk_90008080 {
	std::array<float_t, 4> vec;
};

struct SamplerBind {
	Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler;
	int64_t slot;
};

class Material
{
public:
	D2VertexShader vs;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> vs_textures;
	D2PixelShader ps;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> ps_textures;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cbuffer_ps;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cbuffer_ps_fallback;
	void InitializeCBuffer(ID3D11Device* device, UINT byteWidth, TagHash cbuffer);
	std::vector<SamplerBind> MatSamplers;
	void InitializeSampler(ID3D11Device* device, UT_SamplerRaw sampTag, int64_t slot);
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vs_srv_t0; // register(t0)
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> vs_srv_t1; // register(t1)
	void InitializeCBufferFallback(ID3D11Device* device, UINT byteWidth, std::vector<Unk_90008080> fallbackBytes);
};

