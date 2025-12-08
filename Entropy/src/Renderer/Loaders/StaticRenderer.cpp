#include "StaticRenderer.h"
#include "Renderer/Graphics/Graphics.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/Technique.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Map/TigerBuffer.h"

// Your parser + types
#include "TigerEngine/tag.h"      
#include "TigerEngine/Map/static.h"

using namespace DirectX;


//uint32_t StaticRenderer::RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
//    UINT bindFlags, UINT stride)
//{
//    if (id == 0xFFFFFFFFu) return 0;
//
//    if (!gfx_.registry->HasBuffer(id)) {
//        BufferPayload p{};
//        p.desc.Usage = D3D11_USAGE_DEFAULT;
//        p.desc.BindFlags = bindFlags;
//        p.desc.ByteWidth = static_cast<UINT>(size);
//        p.desc.CPUAccessFlags = 0;
//        p.desc.MiscFlags = 0;
//        p.stride = stride;
//
//        const uint8_t* b = static_cast<const uint8_t*>(bytes);
//        p.data.assign(b, b + size);
//        gfx_.registry->RegisterBuffer(id, std::move(p));
//    }
//    else {
//        // make sure SRV bind intent isn’t lost later
//        auto payload = gfx_.registry->GetBuffer(id);
//        const UINT newFlags = payload.desc.BindFlags | bindFlags;
//        if (newFlags != payload.desc.BindFlags) {
//            payload.desc.BindFlags = newFlags;
//            gfx_.registry->RegisterBuffer(id, std::move(payload)); // update
//        }
//    }
//    return id;
//}

RenderStatic StaticRenderer::Build()
{
    // Construct RenderStatic with required arguments
    // Assuming RenderStatic requires a reference to techniques vector
    auto static_tag = TagHash(static_hash_);
    auto s = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);

    auto mesh_tag = TagHash(s.opaque_meshes);
    auto m = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);

    // Use s.Techniques as the reference for techniques
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
