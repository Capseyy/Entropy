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

	// parse header
	const auto vcbh = bin::parse<VertexBufferHeader>(tag.buffer.data, tag.buffer.size, bin::Endian::Little);
	const void* vcRef = TagHash(tag.buffer.reference).data;

	// IMPORTANT: add SRV bind (VB | SRV)
	const uint32_t colId = RegisterBufferBlob(
		vcRef, vcbh.dataSize, h,
		D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE,
		vcbh.stride
	);

	// stride straight from header; no registry round-trip needed
	const UINT stride = vcbh.stride;

	BufferSRVMeta meta{};
	meta.typedFormat = (stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	meta.bytesPerElement = stride;

	// create / fetch SRV
	auto srvRes = gfx.assets->EnqueueBufferSRV(colId, meta).future.get();

	// Store the SRV properly (ComPtr move). Adjust type if yours differs.
	out.ao_buffer = std::move(srvRes->srv);
	out.AO_stride = stride;

	// Build id->offset map
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
	/*load_datatable_into_scene(TagHash(0x80D2771C));
	load_datatable_into_scene(TagHash(0x80D26815)); 
	load_datatable_into_scene(TagHash(0x80D271D9));
	load_datatable_into_scene(TagHash(0x80E68E50));*/
	printf("Loaded %d datatables\n", data_tables.size());
}

void LoadZone::load_datatable_into_scene(TagHash table) {
	//printf("Starting parse for %08x \n", table.hash);
	const auto datatable = bin::parse<SMapDataTable>(table.data, table.size);

	for (const auto& entry : datatable.data_tables) {
		switch (entry.resource.type)
		{
		case 0x80806cc9: {
			printf("Found static placement\n");
			auto const resource = entry.resource.Parse<Unk_80806CC9>(table);
			const auto static_parent = bin::parse<s_static_map_parent>(resource.static_parent.data, resource.static_parent.size);
			std::unique_ptr<StaticMap> staticMap;
			staticMap = std::make_unique<StaticMap>(gfx);
			staticMap->Initialize(static_parent.static_data.hash);  // root map hash (or whatever yours is)//0x80AC5F28 duality calus //veil 0x8104113E
			auto staticsToDraw = staticMap->GetRenderList();
			for (const auto& static_ : staticsToDraw) {
				this->statics.push_back(static_);
			}
			break;
		}
		case 0x80806a63: {
			printf("Found light placement\n");
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
		case 0x80806a40: {// Ambient OCclusion placementP
			printf("Found AO placement\n");
			auto const resource = entry.resource.Parse<Unk_80806A40>(table);
			auto ao_parent = bin::parse<SAmbientOcclusionParent>(resource.ambient_occlusion.data, resource.ambient_occlusion.size);
			auto ao_map1 = LoadAmbAO(ao_parent.offset_mappings1);
			this->AOMap1 = ao_map1;
			auto ao_map2 = LoadAmbAO(ao_parent.offset_mappings2);
			auto ao_map3 = LoadAmbAO(ao_parent.offset_mappings3);
			break;

		}
		case 0x80806aa3: {
			printf("Found Sky placement in %08X \n", table.hash);
			auto resource = entry.resource.Parse<Unk_80806AA3>(table);
			printf("Sky Ent Tag: %08X \n", resource.sky_ents.hash);
			auto header = bin::parse<Unk_80806AA7>(resource.sky_ents.data, resource.sky_ents.size);

			const size_t n = std::min({ header.unk8.size(), header.unk18.size(), header.unk28.size() });

			for (size_t i = 0; i < n; ++i) {
				const auto& u8 = header.unk8[i];
				const auto& u18 = header.unk18[i];

				if (u8.unk70 == 5) continue;

				// u8.transform is std::array<float, 16> in column-major order
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
				//rot = glm::conjugate(rot); // to match engine convention
				glm::vec4 pos(translation, s_uniform);

				auto sky_entity = bin::parse<Unk_80806AAE>(u8.unk60.data, u8.unk60.size);
				load_entity_model_into_scene(sky_entity.sem, rot, pos, {}, {});
			}
			break;
		}
		default:
			break;
		}
		load_entity_into_scene(TagHash(entry.entity.tagHash32), entry.rotation, entry.translation);



	}
}