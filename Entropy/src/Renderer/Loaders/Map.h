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
#include "TigerEngine/Entity/entity.h"                   // std::cbrt, std::abs


class Graphics;
struct RenderStatic; // if not already included, forward-declare is fine for member usage
struct RenderLight;

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
	void load_datatable_into_scene(TagHash);
	void load_entity_into_scene(TagHash tag, glm::quat quat, glm::vec4 pos);
	void load_entity_model_into_scene(TagHash sem, glm::quat rot, glm::vec4 pos, std::vector<Unk_808072C5> tech_maps, std::vector<TagHash> ext_techs);
	uint32_t RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
		UINT bindFlags, UINT stride = 0);
	MapStaticAO LoadAmbAO(SAmbientOcclusionBuffer tag);
	MapStaticAO AOMap1;
	Graphics& gfx;
};