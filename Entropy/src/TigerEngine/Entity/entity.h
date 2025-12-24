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

struct SDynamicMeshPart
{
	TagHash technique;
	uint16_t varient_shader_index;
	uint8_t PrimitiveType;
	uint8_t unk7;
	uint32_t index_start;
	uint32_t index_count;
	uint32_t unk10;
	uint16_t external_identifier;
	uint16_t unk16;
	uint32_t flags;
	uint8_t gear_dye_change_color_index;
	uint8_t LodCatagory;
	uint8_t unk1e;
	uint8_t lod_run;
	uint32_t unk20;

};

struct SDynamicMesh {
	TagHash vertex0_buffer;
	TagHash vertex1_buffer;
	TagHash buffer2;
	TagHash buffer3;
	TagHash index_buffer;
	TagHash colour_buffer;
	TagHash skinning_buffer;
	uint32_t unk1c;
	std::vector<SDynamicMeshPart> parts;
	std::array<uint16_t, 25> part_range_per_render_stage;
	std::array<uint8_t, 24> input_layout_per_render_stage;
	std::array<uint16_t, 3> pad;

};

struct SEntityModel {
	uint64_t file_size;
	uint64_t unk08;
	std::vector< SDynamicMesh> parts;
	glm::vec4 unk20;
	SkipTo<0x50> skip;
	glm::vec4 model_scale;
	glm::vec4 model_offset;
	glm::vec2 texcoord_scale;
	glm::vec2 texcoord_offset;
};

struct Unk_808072C5
{
	uint32_t technique_count;
	uint32_t technique_start;
	uint32_t unk08;
};

struct Unk_80806D8F  // Entity Mesh Resource 
{
    SkipTo<0x244> Unk0;            
    TagHash MeshFile;                      
	SkipTo<0x330> Unk248;           
	TagHash texplates;
	SkipTo<0x3E0> Unk334;
    std::vector<Unk_808072C5> entity_material_map;  
	SkipTo<0x420> Unk3f0;                     
    std::vector<uint32_t> materials;                          
};


struct SEntityResource
{
	uint64_t file_size;
	ResourcePointer resource08;
	ResourcePointer resource10;
	ResourcePointer resource18;
	SkipTo<0x60> pad;
	std::vector<ResourcePointer> relative_table;
};

struct Unk_80809c04
{
	TagHash entity_resource;
	uint32_t unk04;
	uint32_t unk07;
};
struct Unk_80806AAE
{
	uint64_t FileSize;
	TagHash sem;
};

struct STerrainPart
{
	uint32_t Technique;
	uint32_t IndexStart;
	uint16_t IndexCount;
	uint8_t groupIndex;
	uint8_t detailLevel;
};

struct STerrainGroup
{
	Vec4 Unk0;
	float_t Unk10;
	float_t Unk14;
	float_t Unk18;
	uint32_t Unk1C;
	Vec4 Unk20;
	uint32_t Unk30;
	uint32_t Unk34;
	uint32_t Unk38;
	uint32_t Unk3C;
	uint32_t Unk40;
	uint32_t Unk44;
	uint32_t Unk48;
	uint32_t Unk4C;
	TagHash dyemap;
	uint32_t unk54;
	uint32_t unk58;
	uint32_t unk5c;


};

struct STerrain
{
	uint64_t filesize;
	uint64_t unk8;
	Aabb bounds;
	Vec4 transform;
	SkipTo<0x50> skip;
	std::vector<STerrainGroup> mesh_groups;
	//std::array<uint32_t, 4> mesh_groups_skip;
	TagHash Vertex0;
	TagHash Vertex1;
	TagHash IndexBuffer;
	uint32_t Unk_technique;
	uint32_t Unk_technique2;
	SkipTo<0x78> unk78;
	std::vector<STerrainPart> mesh_parts;

};

struct Unk_80806C7D
{
	uint64_t unk0;
	uint64_t unk8;
	uint32_t unk10;
	uint32_t unk14;
	TagHash terrain_tag;
	TagHash occlusion_bound_tag;
};

struct Unk_808091F1
{
	std::array<uint32_t, 4> unk0;
	ResourcePointer unk10;
};

struct Unk_80808179
{
	std::array<uint32_t,118> pad;
	std::vector<Unk_808091F1> unk10;
};

struct Unk_808067BB
{
	std::array<uint32_t, 4> unk0;
	TagHash particle_system;
};

struct SParticleSystem 
{
	TagHash particle_data_tag;
	std::array<uint32_t, 5> unk8;
	std::uint32_t technique_hash;
	uint32_t unk1C;
	WideHash particle_mesh;
};

struct Unk_80806F06
{
	TagHash sem;
};

struct Unk_80806929
{
	uint64_t FileSize;
	uint64_t unk08;
	std::vector<Unk_80806F06> unk10;
};

struct Unk_808067B9
{
	std::array<uint32_t, 10> unk0;
	std::vector<Unk_808067BB> particle_systems;
};

struct SEntity
{
	uint64_t FileSize;
	std::vector< Unk_80809c04> resources;
};
