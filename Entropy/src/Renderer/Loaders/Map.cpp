#include "Map.h"
#include "Renderer/Graphics/Graphics.h"
#include "TigerEngine/Map/TigerBuffer.h"
#undef min
#undef max
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>         // glm::make_mat4
#include <glm/gtx/matrix_decompose.hpp> // glm::decompose
#include <glm/gtc/quaternion.hpp>
#include <cmath>


MapStaticAO LoadZone::LoadAmbAO(SAmbientOcclusionBuffer tag)
{
	MapStaticAO out{};
	const uint32_t h = tag.buffer.hash;
	if (h == 0u || h == 0xFFFFFFFFu) return out; // no AO buffer

	const auto vcbh = bin::parse<VertexBufferHeader>(tag.buffer.data, tag.buffer.size, bin::Endian::Little);
	const void* vcRef = TagHash(tag.buffer.reference).data;

	const uint32_t colId = RegisterBufferBlob(
		vcRef, vcbh.dataSize, h,
		D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE,
		vcbh.stride
	);

	const UINT stride = vcbh.stride;

	BufferSRVMeta meta{};
	meta.typedFormat = (stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	meta.bytesPerElement = stride;

	auto srvRes = gfx.assets->EnqueueBufferSRV(colId, meta).future.get();

	out.ao_buffer = std::move(srvRes->srv);
	out.AO_stride = stride;

	out.offsets.reserve(tag.offset_mappings.size());
	for (const auto& m : tag.offset_mappings)
		out.offsets.try_emplace(m.identifier, m.offset);

	return out;
}


void LoadZone::ProcessMap()
{
	auto parentTag = TagHash(this->parentHash);
	auto parent = bin::parse<s_bubble_parent>(parentTag.data, parentTag.size);
	auto definition = bin::parse<s_bubble_definition>(parent.bubble_definition.data, parent.bubble_definition.size);
	std::vector<TagHash> data_tables;
	for (const auto& container : definition.resources)
	{
		const auto data_table_container = bin::parse<s_map_container>(container.data, container.size);
		for (auto& datatable : data_table_container.data_tables) {
			data_tables.push_back(datatable);
		}
	}
	for (auto& datatable : data_tables)
	{
		load_datatable_into_scene(datatable);
	}
	printf("Loaded %d datatables\n", data_tables.size());
}

inline void LoadZone::load_datatable_into_scene(TagHash table, glm::quat quat_ovd, glm::vec4 pos_ovd, EntityType et) {
	//printf("Starting parse for %08x \n", table.hash);
	auto datatable = bin::parse<SMapDataTable>(table.data, table.size);

	for (const auto& entry : datatable.data_tables) {
		EntityVecPair evp{};
		evp.pos = entry.translation;
		evp.quat = entry.rotation;
		this->loaded_entity_instances[entry.world_id] = evp;
		
		switch (entry.resource.type)
		{
		case 0x80806cc9: {
			//printf("Found static placement\n");
			auto const resource = entry.resource.Parse<Unk_80806CC9>(table);
			const auto static_parent = bin::parse<s_static_map_parent>(resource.static_parent.data, resource.static_parent.size);
			StaticMap staticMap{};
			staticMap.Initialize(static_parent.static_data.hash);
			for (auto& static_ : staticMap.GetRenderList()) {
				this->statics.push_back(static_);
			}
			break;
		}
		case 0x80806a63: {
			//printf("Found light placement\n");
			auto const resource = entry.resource.Parse<Unk_80806A63>(table);
			const auto light_parent = bin::parse<SLightCollection>(resource.light_collection.data, resource.light_collection.size);
			for (int i = 0; i < light_parent.lights.size(); i++)
			{
				RenderLight ls;
				ls.light_matrix = light_parent.lights[i].light_space_transform;
				ls.rot = light_parent.transforms[i].quat;
				ls.pos = light_parent.transforms[i].pos;
				ls.scale = light_parent.transforms[i].scale;
				auto tech = gfx.assets->EnqueueTechnique(light_parent.lights[i].technique_shading);
				ls.technique = tech.get();
				ls.idx = i;
				ls.parent = resource.light_collection.hash;
				ls.unk50 = light_parent.lights[i].unk50;
				this->lights.push_back(ls);

			}
			break;

		}
		case 0x80806c5e: {
			auto const resource = entry.resource.Parse<Unk_80806C5E>(table);
			const auto expensive_light = bin::parse<SShadowingLight>(resource.expensive_light.data, resource.expensive_light.size);
			RenderLight ls;
			ls.idx = 0;
			ls.light_matrix = expensive_light.light_space_transform;
			ls.pos = entry.translation;
			ls.rot = entry.rotation;
			auto tech = gfx.assets->EnqueueTechnique(expensive_light.technique_shading);
			ls.parent = resource.expensive_light.hash;
			ls.technique = tech.get();
			ls.unk50 = expensive_light.unk50;
			this->lights.push_back(ls);
			break;
		}
		case 0x80806a40: {// Ambient OCclusion placementP
			printf("Found AO placement\n");
			auto const resource = entry.resource.Parse<Unk_80806A40>(table);
			auto ao_parent = bin::parse<SAmbientOcclusionParent>(resource.ambient_occlusion.data, resource.ambient_occlusion.size);
			auto ao_map1 = LoadAmbAO(ao_parent.offset_mappings1);
			this->AOMap1 = ao_map1;
			//auto ao_map2 = LoadAmbAO(ao_parent.offset_mappings2);
			//auto ao_map3 = LoadAmbAO(ao_parent.offset_mappings3);
			break;

		}
		case 0x80806aa3: {

			auto resource = entry.resource.Parse<Unk_80806AA3>(table);
	
			auto header = bin::parse<Unk_80806AA7>(resource.sky_ents.data, resource.sky_ents.size);

			const size_t n = std::min({ header.unk8.size(), header.unk18.size(), header.unk28.size() });

			for (size_t i = 0; i < n; ++i) {
				const auto& u8 = header.unk8[i];
				const auto& u18 = header.unk18[i];

				if (u8.unk70 == 5) continue;

				const glm::mat4 M = glm::make_mat4(u8.transform.data());

				glm::vec3 scale, translation, skew;
				glm::vec4 perspective;
				glm::quat rotation;
				if (!glm::decompose(M, scale, rotation, translation, skew, perspective)) {
					rotation = glm::quat(1, 0, 0, 0);
					translation = glm::vec3(0);
					scale = glm::vec3(1);
				}
				const float sx = std::abs(scale.x), sy = std::abs(scale.y), sz = std::abs(scale.z);
				const float s_uniform = std::cbrt(std::max(1e-6f, sx * sy * sz));

				glm::quat rot;
				rot.w = rotation.x;
				rot.x = rotation.y;
				rot.y = rotation.z;
				rot.z = rotation.w;
				glm::vec4 pos(translation, s_uniform);

				auto sky_entity = bin::parse<Unk_80806AAE>(u8.unk60.data, u8.unk60.size);
				load_entity_model_into_scene(sky_entity.sem, rot, pos, {}, {},header.unk18[i].unk0, EntityType::SkyEntity);
			}
			break;
		}
		case 0x80806C7D: {
			auto resource = entry.resource.Parse<Unk_80806C7D>(table);
			auto terrain_data = bin::parse<STerrain>(resource.terrain_tag.data, resource.terrain_tag.size);
			RenderTerrain rt;
			rt.id = resource.terrain_tag.hash;
			rt.meshData = terrain_data;
			rt.occlusion_bounds = terrain_data.bounds;
			this->terrain_patches.emplace_back(rt);

			break;
		}
		default:
			break;
		}
		if (pos_ovd != glm::vec4()) {
			auto th = TagHash(entry.entity.tagHash32);
			load_entity_into_scene(th, quat_ovd, pos_ovd, 0, et);
		}
		else {
			auto th = TagHash(entry.entity.tagHash32);
			load_entity_into_scene(th, entry.rotation, entry.translation, 0, et);
		}
		



	}
}


void LoadZone::load_activity_phase(TagHash table) {
	const auto activity_phase = bin::parse<Unk_80808EBE>(table.data, table.size);
	std::vector<std::pair<Unk_808046B5,TagHash>> spawn_rule_maps_temp;
	for (const auto& entry : activity_phase.activity_resource) {
		const auto ActResTag = TagHash(entry);
		const auto activity_resource_parent = bin::parse<Unk_80808943>(ActResTag.data, ActResTag.size);
		const auto activity_resource = bin::parse<SActivityResource>(activity_resource_parent.activity_resource_tag.data, activity_resource_parent.activity_resource_tag.size);
		auto& activity_resource_tag = activity_resource_parent.activity_resource_tag;
		switch (activity_resource.unk18.type)
		{
		case 0x808092d8: {
			//printf("Found Entity Instance placement\n");
			auto const resource = activity_resource.unk18.Parse<Unk_808092D8>(activity_resource_tag);
			load_datatable_into_scene(resource.data_table,{},{},EntityType::Activity);
			break;
		}
		case 0x808098fa: { //entity names
			auto const resource = activity_resource.unk18.Parse<Unk_808098FA>(activity_resource_tag);
			if (activity_resource.NameFile.hash != 0xFFFFFFFF) {
				std::unordered_map<uint32_t, std::string> file_names;
				const auto name_tag = bin::parse<Unk_8080906b>(activity_resource.NameFile.data, activity_resource.NameFile.size);
				for (const auto& str_entry : name_tag.string_table) {
					if (str_entry.Unk0.type == 0x8080894d) {
						const auto name_ptr = str_entry.Unk0.Parse<Unk_8080894D>(activity_resource.NameFile);
						const auto name = name_ptr.Unk0.name;
						uint32_t fnv = fnv1_32(name);
						file_names.emplace(fnv, name);
					}
				}
				for (const auto& wid_pair : resource.object_groups) {
					auto it = file_names.find(wid_pair.fnvHash);
					if (it != file_names.end()) {
						loaded_entity_names.try_emplace(wid_pair.world_id, it->second);
					}
				}
			}
			break;
		}
		case 0x80804699: { //spawn_rules
			//printf("Spawn Rules instancer\n");
			auto const resource = activity_resource.unk18.Parse<Unk_80804699>(activity_resource_tag);
			std::vector<uint64_t> world_ids;
			for (const auto& spawn_rule_id : resource.spawn_rule_ids) {
				world_ids.push_back(spawn_rule_id.world_id);

			}
			this->spawn_rule_maps[resource.common_values.world_id] = world_ids;
			break;
		}
		case 0x808046b5: { //combatant instancer
			//printf("Combatant Instancer\n");
			auto const resource = activity_resource.unk18.Parse<Unk_808046B5>(activity_resource_tag);
			std::pair<Unk_808046B5, TagHash> pr;
			pr.first = resource;
			pr.second = activity_resource_tag;
			spawn_rule_maps_temp.emplace_back(pr);
			break;
		}
		case 0x80804695: {//comb spawn points

			auto const resource = activity_resource.unk18.Parse<Unk_80804695>(activity_resource_tag);
			for (const auto& entry : resource.spawn_points) {
				if (entry.pointer.type == 0x8080448b) {
					auto const contest_combat = entry.pointer.Parse<Unk_8080448B>(activity_resource_tag);
					if (contest_combat.data_table.success) {
						load_datatable_into_scene(contest_combat.data_table, {}, {}, EntityType::Combatant);
					}
					
				}
				
			}
			break;
		}

		default:
			break;
		}
	}
	for (const auto& spawn_pair : spawn_rule_maps_temp) {
		const auto& resource = spawn_pair.first;
		const auto& activity_resource_tag = spawn_pair.second;
		for (const auto& combatant : resource.combatant_instances) {
			bool loaded = false;
			if (combatant.entity_data_table.success) {
				auto it = this->spawn_rule_maps.find(resource.sr_id);
				if (it != this->spawn_rule_maps.end()) {
					for (const auto& wid : it->second) {
						auto ev_it = this->loaded_entity_instances.find(wid);
						if (ev_it != this->loaded_entity_instances.end()) {
							glm::vec4 pos = ev_it->second.pos;
							pos.w = 1.0f;
							glm::quat rot = ev_it->second.quat;
							loaded = true;
							load_datatable_into_scene(TagHash(combatant.entity_data_table.tagHash32), rot, pos, EntityType::Combatant);
						}
					}
				}
			}
			else {
				if (combatant.unk68.type == 0x8080462b) {
					const auto unk_462b = combatant.unk68.Parse<Unk_8080462B>(activity_resource_tag);
					for (const auto& instance : unk_462b.combatant_instances) {
						auto it = this->spawn_rule_maps.find(resource.sr_id);
						if (it != this->spawn_rule_maps.end()) {
							for (const auto& wid : it->second) {
								auto ev_it = this->loaded_entity_instances.find(wid);
								if (ev_it != this->loaded_entity_instances.end()) {
									glm::vec4 pos = ev_it->second.pos;
									pos.w = 1.0f;
									glm::quat rot = ev_it->second.quat;
									loaded = true;
									load_datatable_into_scene(TagHash(instance.entity_data_table.tagHash32), rot, pos, EntityType::Combatant);
								}
							}
						}
					}
				}
				else if (combatant.unk68.type == 0x80804690) { //used for contest combatants
					const auto unk_4690 = combatant.unk68.Parse<Unk_80804690>(activity_resource_tag);
					if (activity_resource_tag.hash == 0x8137F6FE) {
						int u = 1;
					}
					for (const auto& instance : unk_4690.combatant_instances) {
						auto it = this->spawn_rule_maps.find(resource.sr_id);
						if (it != this->spawn_rule_maps.end()) {
							for (const auto& wid : it->second) {
								auto ev_it = this->loaded_entity_instances.find(wid);
								if (ev_it != this->loaded_entity_instances.end()) {
									glm::vec4 pos = ev_it->second.pos;
									pos.w = 1.0f;
									glm::quat rot = ev_it->second.quat;

									loaded = true;
									load_datatable_into_scene(TagHash(instance.entity_data_table.tagHash32), rot, pos, EntityType::Combatant);
								}
							}
						}
						if (loaded) {
							break;
						}
					}
				}
			}
			if (!loaded && combatant.entity_data_table.success) {
				printf("Failed to locate position for combatant in tag %08X at default pos\n", activity_resource_tag.hash);
				load_datatable_into_scene(TagHash(combatant.entity_data_table.tagHash32), resource.default_rot, resource.default_pos, EntityType::Combatant);
			}

		}
	}


	/*for (const auto& name_pair : loaded_entity_names) {
		printf("Loaded Entity Name: %s for World ID: %llu\n", name_pair.second.c_str(), name_pair.first);
	}*/
}
