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
#include "Runtime/Assets/AssetSystem.h"

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

struct Unk_80806C5E {//lights
	uint64_t unk0;
	uint64_t unk8;
	TagHash expensive_light;
	uint64_t unk14;
	uint32_t unk1c;
};

struct Unk_80806A40 {//AO
	uint64_t unk0;
	uint64_t unk8;
	TagHash ambient_occlusion;
};

struct Unk_80806AA3 {//AO
	uint64_t unk0;
	uint64_t unk8;
	TagHash sky_ents;
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

struct SAmbientOcclusionOffsetMapping {
	uint64_t identifier;
	uint32_t offset;
	uint32_t unkc;
	std::array<uint32_t, 4> unk10;
};

struct SAmbientOcclusionBuffer {
	TagHash buffer;
	uint32_t unk04;
	std::vector< SAmbientOcclusionOffsetMapping> offset_mappings;
};


struct SMapDataTable {
	uint64_t filesize;
	std::vector<SMapEntry> data_tables;
};

struct SAmbientOcclusionParent {
	uint64_t file_size;
	SAmbientOcclusionBuffer offset_mappings1;
	SAmbientOcclusionBuffer offset_mappings2;
	SAmbientOcclusionBuffer offset_mappings3;
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


	uint32_t  unkc0;

	TagHash   technique_shading;
	TagHash   technique_volumetrics;
	TagHash   technique_compute_lightprobe;
	TagHash   unkd0;
	TagHash   unkd4; 
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

struct SShadowingLight {
	Vec4      unk0;
	Vec4      unk10;
	Vec4      unk20;
	Vec4      unk30;
	std::array<uint32_t,4>  unk40;
	Vec4      unk50;
	glm::mat4 light_space_transform; // 16 floats

	uint32_t  unka0;
	uint32_t  unka4;
	uint32_t  unka8;
	float     unkac;
	float     unkb0;
	float     unkb4;
	float     unkb8;
	float     unkbc;
	float     far_plane;
	float     half_fov;

	uint32_t  unkc8;
	float     unkcc;

	TagHash   technique_shading;
	TagHash   technique_shading_shadowing;
	TagHash   technique_volumetrics;
	TagHash   technique_volumetrics_shadowing;
	TagHash   technique_compute_lightprobe;
	TagHash   technique_compute_lightprobe_shadowing;

	TagHash   unke8; // Unk80806da1
	TagHash   unkec; // Unk80806da1

	float     unkf0[5];
	uint8_t   unk104[12];
};

struct MapStaticAO {
public:
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> ao_buffer;                       // big packed AO vertex blob
	std::unordered_map<uint64_t, uint32_t> offsets;  // id -> offset (units: vertices or bytes)
	UINT AO_stride = 0; // bytes per vertex

	// convenience
	inline bool TryGetOffset(uint64_t id, uint32_t& out) const {
		auto it = offsets.find(id);
		if (it == offsets.end()) return false;
		out = it->second;
		return true;
	}
};

struct AoSlice {
	uint32_t first;   // elements (bytes for R8_UNORM)
	uint32_t count;   // elements
};


inline Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>
MakeSrvSlice(ID3D11Device* dev,
	ID3D11ShaderResourceView* baseSrv,
	UINT firstElement,   // start index (elements, not bytes)
	UINT numElements)    // length  (elements)
{
	Microsoft::WRL::ComPtr<ID3D11Resource> res;
	baseSrv->GetResource(&res);

	D3D11_SHADER_RESOURCE_VIEW_DESC desc{};
	baseSrv->GetDesc(&desc);

	// keep same format (R8_UNORM per your capture), just change the range
	desc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
	desc.Buffer.FirstElement = firstElement;
	desc.Buffer.NumElements = numElements;

	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> slice;
	HRESULT hr = dev->CreateShaderResourceView(res.Get(), &desc, &slice);
	return SUCCEEDED(hr) ? slice : nullptr;
}

struct Unk_80806AA9 {
	std::array<float_t, 16> transform;
	std::array<uint32_t,8> unk40;
	TagHash unk60;
	std::array<uint32_t, 3> unk64;
	uint32_t unk70;
	std::array<uint32_t, 7> unk6c;
};

struct Unk_80806AA7 {
	uint64_t file_size;
	std::vector<Unk_80806AA9> unk8;
	std::vector<SObjectOcclusionBounds> unk18;
	std::vector<uint32_t> unk28;
};