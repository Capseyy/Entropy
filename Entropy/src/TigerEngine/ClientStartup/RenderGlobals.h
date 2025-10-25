#pragma once
#include "TigerEngine/String/string.h"
#include "TigerEngine/Technique/technique.h"
#include "TigerEngine/globaldata.h"
#include <string>
#include <string_view>
#include <iostream>
#include <unordered_map>


struct SScopeStage
{
	std::array<uint32_t, 4> _unk0;
	uint64_t _unk10;
	std::vector<Unk_09008080> TFX_Bytecode;
	std::array<float_t, 4> TFX_Constants; //TODO
	std::vector<Unk_3f018080> Samplers; //TODO#
	std::vector<Unk_90008080> SamplerFallback; //TODO
	std::array<uint32_t, 4> Unk48; //TODO
	int32_t constant_buffer_slot;
	TagHash contstant_buffer;
};


struct SScope
{
	uint64_t filesize;
	RawStringPointer64 name;
	std::array<uint32_t, 14> _unk10;
	SScopeStage stage_pixel;
	SScopeStage stage_vertex;
	SScopeStage stage_geometry;
	SScopeStage stage_hull;
	SScopeStage stage_compute;
	SScopeStage stage_domain;

};

struct Unk_808067AD
{
	RawStringPointer64 name;
	uint32_t _unk8;
	TagHash scope;
};

struct Unk_808067AC
{
	RawStringPointer64 name;
	uint32_t _unk8;
	TagHash tech;
};


struct Unk_808067A8
{
	uint64_t filesize;
	TagHash _unk8;
	uint32_t _unkc;
	std::vector<Unk_808067AD> scopes;
	std::vector< Unk_808067AC> techniques;
};

struct Unk_8080870F
{
	uint32_t _unk0;
	uint32_t _unk4;
	TagHash tag;
	uint32_t _unkc;
};

struct Unk_8080978C
{
	uint64_t filesize;
	std::vector<Unk_8080870F> entries;
};

bool GenerateRenderGlobals();