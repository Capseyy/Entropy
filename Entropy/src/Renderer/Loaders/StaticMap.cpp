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
		if (static_instancer.static_tags[static_instance.static_intex].reference != 0x80806D44)
		{
			continue;
		}
		StaticRenderer renderer(gfx_, static_instancer.static_tags[static_instance.static_intex]);
		auto renderpart = renderer.Build();
		for (int i = static_instance.instance_start; i < static_instance.instance_start + static_instance.instance_count; i++) {
			auto& transform = static_instancer.instance_transforms[i];
			renderpart.world.push_back(transform);
		}
		statics_.push_back(renderpart);
	}
	return true;
}

void StaticMap::LoadAll_Statics()
{
	int u = 1;
}

std::vector<RenderStatic> StaticMap::GetRenderList()
{
	return statics_;
}
