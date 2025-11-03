#pragma once
#include <string>
#include <unordered_map>
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <execution>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>  
#include "Renderer/Loaders/StaticMap.h"
#include "occlusion.h"
#include "TigerEngine/Entity/entity.h"

struct s_bubble_parent {
	uint64_t filesize;
	WideHash bubble_definition;
	uint64_t unk18;
	uint32_t map_name;

};

struct s_static_map_parent
{
	uint64_t unk0;
	TagHash static_data;
};

struct Unk_80806CC9 {//statics
	uint64_t unk0; 
	uint64_t unk8;
	TagHash static_parent;
};

struct Unk_80806A63 {//lights
	uint64_t unk0;
	uint64_t unk8;
	TagHash light_collection;
};
struct s_bubble_definition {
	uint64_t filesize;
	std::vector<WideHash> resources;
};

struct s_map_container {
	uint64_t filesize;
	std::array<uint64_t, 4> _unk4;
	std::vector<TagHash> data_tables;
};

struct SMapEntry
{
	glm::quat rotation;
	glm::vec4 translation;
	uint64_t unk20;
	WideHash entity;
	std::array<uint32_t, 14> unk38;
	uint64_t world_id;
	ResourcePointer resource{};
	std::array<uint32_t, 4> unk80;
};

struct SMapDataTable {
	uint64_t filesize;
	std::vector<SMapEntry> data_tables;
};

struct SLight {
	Vec4 unk0;
	Vec4 unk10;
	Vec4 unk20;
	Vec4 unk30;
	uint32_t  unk40[4];
	Vec4 unk50;
	glm::mat4 light_space_transform; // 16 floats
	uint32_t  unka0;
	uint32_t  unka4;
	uint32_t  unka8;
	float     unkac;
	float     unkb0;
	float     unkb4;
	float     unkb8;
	float     unkbc;

	// New in TFS (taghash-like value e.g. 0x9E440E84)
	uint32_t  unkc0;

	TagHash   technique_shading;
	TagHash   technique_volumetrics;
	TagHash   technique_compute_lightprobe;
	TagHash   unkd0; // Unk80806da1
	TagHash   unkd4; // Unk80806da1
	uint32_t  unkd8[6];
};

struct Unk_80809F4F
{
	glm::quat quat;
	glm::vec3 pos;
	float_t scale;
};

struct SLightCollection {
	uint64_t file_size;
	uint64_t unk8;
	Aabb bounds;
	std::vector<SLight> lights;
	std::vector<Unk_80809F4F> transforms;

};