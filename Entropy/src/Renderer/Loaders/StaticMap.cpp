// StaticMap.cpp
#include "StaticMap.h"
#include "Renderer/Graphics/Graphics.h"
#include "StaticRenderer.h"
#include "TigerEngine/Map/static.h"
// #include "TigerEngine/Map/..."  // if you actually parse the root

StaticMap::StaticMap(Graphics& gfx) : gfx_(gfx) {}

bool StaticMap::Initialize(uint32_t mapRootHash)
{
	auto StaticInstanceTable = TagHash(mapRootHash);
	auto static_instancer = bin::parse<SStaticMeshInstances>(StaticInstanceTable.data, StaticInstanceTable.size, bin::Endian::Little);
	printf("Static Instance Table has %zu static tags\n", static_instancer.static_tags.size());
	for (auto& static_instance : static_instancer.instance_groups) {
		StaticRenderer renderer(gfx_, static_instancer.static_tags[static_instance.static_intex]);
		renderer.Build();
		for (int i = static_instance.instance_start; i < static_instance.instance_start + static_instance.instance_count; i++) {
			auto& transform = static_instancer.instance_transforms[i];
		}
	}
	return true;
}

void StaticMap::LoadAll_Statics()
{
	int u = 1;
}
