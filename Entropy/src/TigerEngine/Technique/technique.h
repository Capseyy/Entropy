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
	std::vector<Unk_90008080> SamplerFallback; //TODO
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
	Material Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements);
};


