#include "StaticRenderer.h"
#include "Renderer/Graphics/Graphics.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/Technique.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Map/TigerBuffer.h"


#include "TigerEngine/tag.h"      
#include "TigerEngine/Map/static.h"

using namespace DirectX;
































RenderStatic StaticRenderer::Build()
{

    auto static_tag = TagHash(static_hash_);
    auto s = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);

    auto mesh_tag = TagHash(s.opaque_meshes);
    auto m = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);

    
    RenderStatic out{};

    out.mesh = m;
    
    out.techniques = s.Techniques;
    out.id = static_hash_.hash;
    for (const auto& special : s.special_meshes) {
		StaticSpecial smOut{};
		smOut.id = static_hash_.hash;
		smOut.part = special;
		out.specials.push_back(smOut);
    }

    return out;
}
