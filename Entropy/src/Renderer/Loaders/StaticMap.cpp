
#include "StaticMap.h"
#include "Renderer/Graphics/Graphics.h"
#include "StaticRenderer.h"
#include "TigerEngine/Map/static.h"


bool StaticMap::Initialize(uint32_t mapRootHash)
{
    const TagHash staticInstanceTable(mapRootHash);

    const auto static_instancer =
        bin::parse<SStaticMeshInstances>(staticInstanceTable.data, staticInstanceTable.size, bin::Endian::Little);

    const auto occlusionTag =
        bin::parse<SOcclusionBounds>(static_instancer.occlusionBounds.data, static_instancer.occlusionBounds.size);

    statics_.clear();
    statics_.reserve(static_instancer.instance_groups.size());

    const bool hasOcclMap = !static_instancer.occlusion_map.empty();

    for (const auto& grp : static_instancer.instance_groups)
    {

        StaticRenderer renderer(static_instancer.static_tags[grp.static_intex]);
        RenderStatic renderpart = renderer.Build();

        const int start = grp.instance_start;
        const int end = start + grp.instance_count;
        const size_t count = static_cast<size_t>(grp.instance_count);

        
        renderpart.world.resize(count);
        renderpart.bounds.resize(count);

        if (!hasOcclMap) {
            for (int i = start, j = 0; i < end; ++i, ++j) {
                renderpart.world[j] = static_instancer.instance_transforms[i].transform;
                renderpart.bounds[j] = occlusionTag.bounds[i].unk0;
            }
        }
        else {
            for (int i = start, j = 0; i < end; ++i, ++j) {
                renderpart.world[j] = static_instancer.instance_transforms[i].transform;
                const int mappedIdx = static_instancer.occlusion_map[i];
                renderpart.bounds[j] = occlusionTag.bounds[mappedIdx].unk0;
            }
        }

        renderpart.AOID = static_instancer.unk98;

        
        statics_.emplace_back(std::move(renderpart));
    }

    return true;
}


const std::vector<RenderStatic>& StaticMap::GetRenderList() const
{
    return statics_;
}
