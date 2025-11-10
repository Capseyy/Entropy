#include "StaticRenderer.h"
#include "Renderer/Graphics/Graphics.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/Technique.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Map/TigerBuffer.h"

// Your parser + types
#include "TigerEngine/tag.h"       // TagHash(uint64_t) -> { data, size }
#include "TigerEngine/Map/static.h"// SStaticModel, SStaticMeshData

using namespace DirectX;

StaticRenderer::StaticRenderer(Graphics& gfx, TagHash static_hash)
    : gfx_(gfx), static_hash_(static_hash) {
}

uint32_t StaticRenderer::RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
    UINT bindFlags, UINT stride)
{
    if (id == 0xFFFFFFFFu) return 0;

    if (!gfx_.registry->HasBuffer(id)) {
        BufferPayload p{};
        p.desc.Usage = D3D11_USAGE_DEFAULT;
        p.desc.BindFlags = bindFlags;
        p.desc.ByteWidth = static_cast<UINT>(size);
        p.desc.CPUAccessFlags = 0;
        p.desc.MiscFlags = 0;
        p.stride = stride;

        const uint8_t* b = static_cast<const uint8_t*>(bytes);
        p.data.assign(b, b + size);
        gfx_.registry->RegisterBuffer(id, std::move(p));
    }
    else {
        // make sure SRV bind intent isn’t lost later
        auto payload = gfx_.registry->GetBuffer(id);
        const UINT newFlags = payload.desc.BindFlags | bindFlags;
        if (newFlags != payload.desc.BindFlags) {
            payload.desc.BindFlags = newFlags;
            gfx_.registry->RegisterBuffer(id, std::move(payload)); // update
        }
    }
    return id;
}

RenderStatic StaticRenderer::Build()
{
    // --- Parse model + mesh ---
    auto static_tag = TagHash(static_hash_);
    auto s = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);

    auto mesh_tag = TagHash(s.opaque_meshes);
    auto m = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);

    // LOD pass
    int max_detail = 0xff;
    for (const auto& group : m.mesh_groups)
        if (group.TfxRenderStage < max_detail) max_detail = group.TfxRenderStage;

    // Pre-extract buffer refs + derived info from headers (no registry queries later)
    struct GroupRef {
        uint32_t posId{}, idxId{}, uvId{}, colId{};
        uint32_t vertexStride{}, uvStride{}, colorStride{};
        uint32_t indexCount{};
        DXGI_FORMAT indexFormat{ DXGI_FORMAT_UNKNOWN };
        bool hasPos{}, hasIdx{}, hasUv{}, hasCol{};
    };
    std::vector<GroupRef> groupRefs;
    groupRefs.reserve(m.buffers.size());

    auto registerVB = [&](const auto& vb, uint32_t& idOut, uint32_t& strideOut, bool& has) {
        if (vb.hash == 0xFFFFFFFFu) { has = false; return; }
        const auto vbh = bin::parse<VertexBufferHeader>(vb.data, vb.size, bin::Endian::Little);
        const void* bytes = TagHash(vb.reference).data;
        idOut = RegisterBufferBlob(bytes, vbh.dataSize, vb.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        strideOut = vbh.stride;
        has = (idOut != 0);
        };
    auto registerIB = [&](const auto& ib, uint32_t& idOut, uint32_t& idxCountOut, DXGI_FORMAT& fmtOut, bool& has) {
        if (ib.hash == 0xFFFFFFFFu) { has = false; return; }
        const auto ibh = bin::parse<IndexBufferHeader>(ib.data, ib.size, bin::Endian::Little);
        const void* bytes = TagHash(ib.reference).data;
        idOut = RegisterBufferBlob(bytes, ibh.dataSize, ib.hash, D3D11_BIND_INDEX_BUFFER, 0);
        const bool idx32 = (ibh.is32 != 0);
        fmtOut = idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
        idxCountOut = idx32 ? (ibh.dataSize / 4u) : (ibh.dataSize / 2u);
        has = (idOut != 0);
        };

    for (const auto& bg : m.buffers) {
        GroupRef gr{};
        registerIB(bg.IndexBuffer, gr.idxId, gr.indexCount, gr.indexFormat, gr.hasIdx);
        registerVB(bg.VertexBuffer, gr.posId, gr.vertexStride, gr.hasPos);
        registerVB(bg.UVBuffer, gr.uvId, gr.uvStride, gr.hasUv);
        // colour buffer needs SRV later; still capture stride now
        if (bg.VertexColourBuffer.hash != 0xFFFFFFFFu) {
            const auto vcbh = bin::parse<VertexBufferHeader>(bg.VertexColourBuffer.data, bg.VertexColourBuffer.size, bin::Endian::Little);
            const void* bytes = TagHash(bg.VertexColourBuffer.reference).data;
            gr.colId = RegisterBufferBlob(bytes, vcbh.dataSize, bg.VertexColourBuffer.hash,
                D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE,
                vcbh.stride);
            gr.colorStride = vcbh.stride;
            gr.hasCol = (gr.colId != 0);
        }
        groupRefs.push_back(gr);
    }

    // Specials: same idea — derive counts/strides from headers; no registry reads.
    UINT highestDetail_special = 99;
    for (const auto& party : s.special_meshes)
        if (party.LodCatagory < highestDetail_special) highestDetail_special = party.LodCatagory;

    std::vector<StaticSpecial> specials;
    specials.reserve(s.special_meshes.size());
    for (const auto& bg : s.special_meshes) {
        if (bg.LodCatagory != highestDetail_special) continue;

        auto grp = std::make_shared<BufferGroup>();

        // index
        if (bg.IndexBuffer.hash != 0xFFFFFFFFu) {
            const auto ibh = bin::parse<IndexBufferHeader>(bg.IndexBuffer.data, bg.IndexBuffer.size, bin::Endian::Little);
            const void* bytes = TagHash(bg.IndexBuffer.reference).data;
            const uint32_t id = RegisterBufferBlob(bytes, ibh.dataSize, bg.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER, 0);
            grp->index = gfx_.assets->EnqueueBuffer(id).future.get();
            grp->indexFormat = (ibh.is32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT);
            grp->indexCount = ibh.is32 ? (ibh.dataSize / 4u) : (ibh.dataSize / 2u);
        }

        // pos
        if (bg.VertexBuffer1.hash != 0xFFFFFFFFu) {
            const auto vbh = bin::parse<VertexBufferHeader>(bg.VertexBuffer1.data, bg.VertexBuffer1.size, bin::Endian::Little);
            const void* bytes = TagHash(bg.VertexBuffer1.reference).data;
            const uint32_t id = RegisterBufferBlob(bytes, vbh.dataSize, bg.VertexBuffer1.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
            grp->vertex = gfx_.assets->EnqueueBuffer(id).future.get();
            grp->vertexStride = vbh.stride;
        }

        // uv
        if (bg.VertexBuffer2.hash != 0xFFFFFFFFu) {
            const auto uvbh = bin::parse<VertexBufferHeader>(bg.VertexBuffer2.data, bg.VertexBuffer2.size, bin::Endian::Little);
            const void* bytes = TagHash(bg.VertexBuffer2.reference).data;
            const uint32_t id = RegisterBufferBlob(bytes, uvbh.dataSize, bg.VertexBuffer2.hash, D3D11_BIND_VERTEX_BUFFER, uvbh.stride);
            grp->uv = gfx_.assets->EnqueueBuffer(id).future.get();
            grp->uvStride = uvbh.stride;
        }

        // color SRV (no AddRef/Release; move the ComPtr the asset system returns)
        if (bg.VertexColourBuffer.hash != 0xFFFFFFFFu) {
            const auto vcbh = bin::parse<VertexBufferHeader>(bg.VertexColourBuffer.data, bg.VertexColourBuffer.size, bin::Endian::Little);
            const void* bytes = TagHash(bg.VertexColourBuffer.reference).data;

            const uint32_t id = RegisterBufferBlob(bytes,
                vcbh.dataSize,
                bg.VertexColourBuffer.hash,
                D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE, // ? add SRV bind
                vcbh.stride);

            BufferSRVMeta meta{};
            meta.typedFormat = (vcbh.stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
            meta.bytesPerElement = vcbh.stride;

            auto srvRes = gfx_.assets->EnqueueBufferSRV(id, meta).future.get();
            grp->color = std::move(srvRes->srv);   // ? move the ComPtr, do NOT call .Get()
            grp->colorStride = vcbh.stride;
        }

        StaticSpecial special{};
        special.group = std::move(grp);
        special.input_layout_index = bg.input_layout_index;
        special.part = bg;
        specials.emplace_back(std::move(special));
    }

    // Enqueue techniques (your dedupe already happens in asset system)
    auto mesh = std::make_shared<StaticMesh>();
    mesh->techniques.resize(s.Techniques.size());
    for (size_t i = 0; i < s.Techniques.size(); ++i)
        mesh->techniques[i] = gfx_.assets->EnqueueTechnique(s.Techniques[i].Unk0).get();

    // Build BufferGroups; spawn threads only if it’s worth it
    mesh->groups.reserve(groupRefs.size());
    auto buildOne = [this](const GroupRef& gr) -> std::shared_ptr<BufferGroup> {
        auto grp = std::make_shared<BufferGroup>();
        if (gr.hasPos) { grp->vertex = gfx_.assets->EnqueueBuffer(gr.posId).future.get(); grp->vertexStride = gr.vertexStride; }
        if (gr.hasIdx) { grp->index = gfx_.assets->EnqueueBuffer(gr.idxId).future.get(); grp->indexFormat = gr.indexFormat; grp->indexCount = gr.indexCount; }
        if (gr.hasUv) { grp->uv = gfx_.assets->EnqueueBuffer(gr.uvId).future.get(); grp->uvStride = gr.uvStride; }
        if (gr.hasCol) {
            BufferSRVMeta meta{};
            meta.typedFormat = (gr.colorStride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
            meta.bytesPerElement = gr.colorStride;
            auto srvRes = gfx_.assets->EnqueueBufferSRV(gr.colId, meta).future.get();
            grp->color = std::move(srvRes->srv);
            grp->colorStride = gr.colorStride;
        }
        return grp;
        };

   
    for (const auto& gr : groupRefs)
        mesh->groups.emplace_back(buildOne(gr));
    

    // Copy over light meta only (avoid copying heavy m if not required)
    mesh->parts = m.parts;
    mesh->meshGroups = m.mesh_groups;
    mesh->id = static_hash_.hash;

    StaticMeshConstants smc;
	smc.mesh_offset = m.mesh_offset;
	smc.max_colour_index = m.max_colour_index;
	smc.mesh_scale = m.mesh_scale;
	smc.texcoord_offset = m.texture_coordinate_offset;
	smc.texcoord_scale = m.texture_coordinate_scale;


    RenderStatic out{};
    out.mesh = std::move(mesh);
    out.meshData = smc;

    // specials techniques
    out.specials.resize(specials.size());
    for (size_t i = 0; i < specials.size(); ++i) {
        auto spec = std::make_shared<StaticSpecial>(specials[i]);
        // technique resolve
        std::shared_ptr<EntropyAssets::Technique> t;
        try { t = gfx_.assets->EnqueueTechnique(spec->part.technique).get(); }
        catch (...) {}
        spec->technique = std::move(t);
        spec->techniqueId = spec->part.technique.hash;
        out.specials[i] = std::move(spec);
    }
    return out;
}
