#pragma once
#include <string>
#include <unordered_map>
#include "TigerEngine/package.h"
#include <future>
#include "TigerEngine/tag.h"
#include <execution>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>  
#include "RenderStatic.h"
#include "RenderLights.h"
#include "RenderEntity.h"
//#include "TigerEngine/Entity/entity.h"                   // std::cbrt, std::abs


class Graphics;
struct RenderStatic; // if not already included, forward-declare is fine for member usage
struct RenderLight;

struct EntityVecPair
{
	glm::vec4 pos;
	glm::quat quat;
};

class LoadZone
{
public:
	explicit LoadZone(Graphics& gfx_) : gfx(gfx_), parentHash(0) {}
	uint32_t parentHash;
	std::string name;
	std::vector<RenderStatic> statics;
	std::vector<RenderLight> lights;
	std::vector<RenderEntity> entities;
	void ProcessMap();
	void load_datatable_into_scene(TagHash, glm::quat quat = {}, glm::vec4 pos = {}, EntityType et = EntityType::Standard);
	void load_entity_into_scene(TagHash& tag, glm::quat quat, glm::vec4 pos, int recursion_depth = 0, EntityType et = EntityType::Standard);
	void load_entity_model_into_scene(TagHash sem, glm::quat rot, glm::vec4 pos, std::vector<Unk_808072C5> tech_maps, std::vector<uint32_t> ext_techs, std::optional<Aabb> Occlusion_Bounds, EntityType et);
	uint32_t RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
		UINT bindFlags, UINT stride = 0);
	MapStaticAO LoadAmbAO(SAmbientOcclusionBuffer tag);
	MapStaticAO AOMap1;
	Graphics& gfx;
	void load_activity_phase(TagHash resource_table);
	std::unordered_map<uint64_t, std::string> loaded_entity_names;
	std::unordered_map<uint64_t, EntityVecPair> loaded_entity_instances; //used for mapping combatants to correct positions
	std::unordered_map<uint64_t, std::vector<uint64_t>> spawn_rule_maps;
};