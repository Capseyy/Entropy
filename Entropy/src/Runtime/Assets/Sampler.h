#pragma once
#include "d3d11.h"
#include "TigerEngine/tag.h"
#include <optional>


struct UT_SamplerRaw
{
	uint32_t Filter;          
	uint32_t AddressU;        
	uint32_t AddressV;
	uint32_t AddressW;
	float    MipLODBias;
	uint32_t MaxAnisotropy;
	uint32_t ComparisonFunc;  
	float    BorderColor[4];
	float    MinLOD;
	float    MaxLOD;          
};

std::optional<D3D11_SAMPLER_DESC> BuildSamplerDescFromTag(TagHash tag);
