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
#include "RenderTerrain.h"
#include <vector>
#include <optional>
#include <cstdint>
#include <unordered_set>

class Graphics;
struct RenderStatic; 
struct RenderLight;
struct RenderTerrain;

struct EntityVecPair
{
	glm::vec4 pos;
	glm::quat quat;
};

struct CachedSpawn
{
	uint32_t sem_hash32 = 0xFFFFFFFFu;                 
	std::vector<Unk_808072C5> tech_maps;
	std::vector<uint32_t> ext_techs;
	std::optional<Aabb> occlusion_bounds;
	EntityType et = EntityType::ChildEntity;
	std::optional<uint32_t> partical_technique;
};

static inline uint64_t MakeEntityCacheKey(uint32_t entity_hash32, uint32_t remainingDepth, EntityType et)
{
	return (uint64_t(entity_hash32) << 32) ^ (uint64_t(remainingDepth) << 8) ^ uint64_t(uint8_t(et));
}

class LoadZone
{
public:
	explicit LoadZone(Graphics& gfx_) : gfx(gfx_), parentHash(0) {}
	uint32_t parentHash;
	std::string name;
	std::vector<RenderStatic> statics;
	std::vector<RenderLight> lights;
	std::vector<RenderEntity> entities;
	std::vector<RenderTerrain> terrain_patches;
	std::unordered_map<uint64_t, std::vector<CachedSpawn>> entity_spawn_cache;
	void ProcessMap();
	void load_datatable_into_scene(TagHash, glm::quat quat = {}, glm::vec4 pos = {}, EntityType et = EntityType::Standard);
	void load_entity_into_scene(TagHash& tag, glm::quat quat, glm::vec4 pos, int recursion_depth = 0, EntityType et = EntityType::Standard, uint64_t world_id = 0);
	void load_entity_model_into_scene(TagHash sem, glm::quat rot, glm::vec4 pos, std::vector<Unk_808072C5> tech_maps, std::vector<uint32_t> ext_techs, std::optional<Aabb> Occlusion_Bounds, EntityType et, std::optional<uint32_t> particle_tech = NULL);
	uint32_t RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
		UINT bindFlags, UINT stride = 0);
	MapStaticAO LoadAmbAO(SAmbientOcclusionBuffer tag);
	MapStaticAO AOMap1;
	Graphics& gfx;
	void load_activity_phase(TagHash resource_table, bool LoadCombatant);
	std::unordered_map<uint64_t, std::string> loaded_entity_names;
	std::unordered_map<uint64_t, EntityVecPair> loaded_entity_instances; //used for mapping combatants to correct positions
	std::unordered_map<uint64_t, std::vector<uint64_t>> spawn_rule_maps;
	std::vector<CachedSpawn> collect_entity_spawns(TagHash entityTag, uint32_t remainingDepth, EntityType et);
	RenderEntity build_render_entity_prototype(
		TagHash sem,
		const std::vector<Unk_808072C5>& tech_maps,
		const std::vector<uint32_t>& ext_techs,
		const std::optional<Aabb>& occlusion_bounds,
		EntityType et,
		std::optional<uint32_t> particle_tech
	);
	std::unordered_map<uint64_t, std::vector<RenderEntity>> entity_render_cache;
	void spawn_from_cached_prototypes(
		const std::vector<RenderEntity>& protos,
		const glm::quat& quat,
		const glm::vec4& pos
	);
};