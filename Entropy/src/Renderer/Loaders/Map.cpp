#include "Map.h"
#include "Renderer/Graphics/Graphics.h"


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
	load_datatable_into_scene(TagHash(0x80D268D7));
	load_datatable_into_scene(TagHash(0x80D26815));
	printf("Loaded %d datatables\n", data_tables.size());
}

void LoadZone::load_datatable_into_scene(TagHash table) {
	printf("Starting parse for %08x \n", table.hash);
	const auto datatable =  bin::parse<SMapDataTable>(table.data, table.size);
	
	for (auto entry : datatable.data_tables) {
		if (entry.resource.type == 0x80806cc9) {
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
		}
		else if (entry.resource.type == 0x80806a63) {
			printf("Found light placement\n");
			auto const resource = entry.resource.Parse<Unk_80806A63>(table);
			const auto light_parent = bin::parse<SLightCollection>(resource.light_collection.data, resource.light_collection.size);
			for (int i = 0; i < light_parent.lights.size();i++)
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

		}
		//if (entry.entity.tagHash32 == 0x810A3E87)
		load_entity_into_scene(TagHash(entry.entity.tagHash32),entry.rotation,entry.translation);
		

	}
}
