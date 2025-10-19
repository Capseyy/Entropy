#pragma once
#include "d3d11.h"
#include "TigerEngine/tag.h"
#include <optional>


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

std::optional<D3D11_SAMPLER_DESC> BuildSamplerDescFromTag(TagHash tag);
