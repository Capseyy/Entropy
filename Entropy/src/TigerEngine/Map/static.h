#pragma once
#include <string>
#include <unordered_map>
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <execution>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>  
#include "Renderer/Graphics/Render/FrustumCulling.h"


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

struct SStaticSpecial {
	uint8_t TfxRenderStage;
	uint8_t input_layout_index;
	uint8_t LodCatagory;
	int8_t _unk4;
	uint8_t PrimitiveType;
	int8_t _unk6;
	uint16_t _unk7;
	TagHash IndexBuffer;
	TagHash VertexBuffer1;
	TagHash VertexBuffer2;
	TagHash VertexColourBuffer;
	uint32_t index_start;
	uint32_t index_count;
	uint32_t technique;
};

struct SStaticModel {
public:
	uint64_t FileSize{};
	TagHash opaque_meshes{};
	uint32_t Unk0C{};
	std::vector<uint32_t> Techniques{};
	std::vector<SStaticSpecial> special_meshes; //todo special_meshes
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
	ObjectVectors transform;
	uint64_t _unk20;
	uint32_t _unk28;
	uint32_t _unk2C;
	std::array<uint32_t,4> unk30;
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
	uint32_t _unk1C;
	std::vector<uint32_t> occlusion_map;
	std::array<uint32_t, 4> _unk1c;
	std::vector<SStaticInstanceTransform> instance_transforms;
	std::array<uint64_t, 5> _unk50;
	std::vector<TagHash> static_tags;
	std::vector<SStaticMeshInstanceGroup> instance_groups;
	uint64_t unk98;
};

inline std::vector<std::uint8_t>
to_bytes(const std::vector<SStaticInstanceTransform>& v)
{
	std::vector<std::uint8_t> bytes;
	bytes.resize(v.size() * sizeof(SStaticInstanceTransform));
	if (!v.empty())
		std::memcpy(bytes.data(), v.data(), bytes.size());
	return bytes;
}
