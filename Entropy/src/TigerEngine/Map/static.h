#pragma once
#include <string>
#include <unordered_map>
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <execution>
#include <glm/glm.hpp>


struct Unk_0x14008080 {
	TagHash Unk0;
};

struct SStaticMeshPart {
public:
	uint32_t index_start;
	uint32_t index_count;
	uint8_t buffer_index;
	uint8_t Unk9;
	uint8_t LodCatagory;
	uint8_t PrimitiveType;
};

struct SStaticMeshBuffers {
public:
	TagHash IndexBuffer;
	TagHash VertexBuffer;
	TagHash UVBuffer;
	TagHash VertexColourBuffer;
};

struct SStaticMeshGroup {
public:
	uint16_t part_index;
	uint8_t TfxRenderStage;
	uint8_t input_layout_index;
	uint8_t Unk5;
	uint8_t Unk6;
};

struct SStaticModel {
public:
	uint64_t FileSize{};
	TagHash opaque_meshes{};
	uint32_t Unk0C{};
	std::vector<Unk_0x14008080> Techniques{};
	std::array<uint32_t, 4> Unk20; //todo special_meshes
	uint32_t Unk30{};
	uint64_t Unk34{};
	std::array<float_t,3> Unk3C;
};

struct SStaticMeshData {
public:
	uint64_t FileSize;
	std::vector<SStaticMeshGroup> mesh_groups;
	std::vector<SStaticMeshPart> parts;
	std::vector<SStaticMeshBuffers> buffers;
	uint64_t Unk38;
	std::array<float_t,3> mesh_offset;
	float_t mesh_scale;
	float_t texture_coordinate_scale;
	std::array<float_t,2> texture_coordinate_offset;
	uint32_t max_colour_index;

};

struct SStaticInstanceTransform
{
	std::array<float_t, 4> rotation;
	std::array<float_t, 3> translation;
	std::array<float_t, 3> scale;
	std::array<float_t, 6> _unk28;
};

struct SStaticMeshInstanceGroup {
	uint16_t instance_count;
	uint16_t instance_start;
	uint16_t static_intex;
	uint16_t unk6;
};

struct SStaticMeshInstances {
	uint64_t FileSize;
	std::array<uint32_t, 4> _unk08;
	TagHash occlusionBounds;
	std::array<uint32_t, 9> _unk1c;
	std::vector<SStaticInstanceTransform> instance_transforms;
	std::array<uint64_t, 5> _unk50;
	std::vector<TagHash> static_tags;
	std::vector<SStaticMeshInstanceGroup> instance_groups;
};