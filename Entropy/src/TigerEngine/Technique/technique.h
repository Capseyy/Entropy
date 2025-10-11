#pragma once

#include <string>
#include <unordered_map>
#include "string.h"
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <iostream>
#include <codecvt>
#include <locale>
#include "Renderer/Graphics/Shaders/Shaders.h"
#include "Renderer/Graphics/Shaders/material.h"

struct STextureTag {
	uint32_t TextureIndex;
	uint32_t Unk04;
	WideHash Texture;
};

struct Unk_09008080 {
public:
	int8_t Unk0;
};

struct Unk_3f018080 {
	TagHash sampler;
	uint32_t Unk4;
	uint64_t Unk8;
};

struct STechniqueShader {
public:
	TagHash ShaderTag;
	uint32_t Unk04;
	std::vector<STextureTag> Textures;
	uint64_t Unk18;
	std::vector<Unk_09008080> TFX_Bytecode;
	std::array<float_t, 4> TFX_Constants; //TODO
	std::vector<Unk_3f018080> Samplers; //TODO#
	std::array<float_t, 4> Unk38; //TODO
	std::array<uint32_t, 4> Unk48; //TODO
	int32_t constant_buffer_slot;
	TagHash contstant_buffer;
	std::array<uint32_t, 6> Unk78; //TODO

};

struct ConstantBufferHeader {
	uint64_t FileSize;
};

class STechnique {
public:
	uint64_t FileSize;
	std::array<uint32_t,6> Unk08;
	uint64_t UsedScopes;
	uint64_t CompatibleScopes;
	uint32_t RenderStates;
	std::array<uint32_t, 15> Unk34;
	STechniqueShader VertexShader;
	STechniqueShader UnkShader1;
	STechniqueShader UnkShader2;
	STechniqueShader GeometryShader;
	STechniqueShader PixelShader;
	STechniqueShader ComputeShader;

	void Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements);
};



struct STextureHeader {
public:
	uint32_t dataSize;
	uint32_t dxgiFormat;
	std::array<uint32_t,6> _unk08;
	uint16_t cafe;
	uint16_t width;
	uint16_t height;
	uint16_t depth;
	uint16_t arraySize;
	uint16_t tileCount;
	uint8_t unk2c;
	uint8_t mipCount;
	std::array<uint8_t,10> _unk2e;
	uint32_t unk38;
	TagHash large_buffer;
};


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


