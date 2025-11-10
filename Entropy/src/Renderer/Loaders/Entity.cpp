#include "Renderer/Loaders/Map.h"
#include "Renderer/Graphics/Graphics.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/Technique.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Map/TigerBuffer.h"
#include "StaticRenderer.h"
#include "TigerEngine/tag.h"       // TagHash(uint64_t) -> { data, size }
#include "TigerEngine/Map/static.h"// SStaticModel, SStaticMeshData
#include <cstdint>
#include "RenderStatic.h"     
#include "RenderEntity.h"     


uint32_t LoadZone::RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
    UINT bindFlags, UINT stride)
{
    if (id == 0xFFFFFFFFu) return 0;

    if (!gfx.registry->HasBuffer(id)) {
        BufferPayload p{};
        p.desc.Usage = D3D11_USAGE_DEFAULT;
        p.desc.BindFlags = bindFlags;
        p.desc.ByteWidth = static_cast<UINT>(size);
        p.desc.CPUAccessFlags = 0;
        p.desc.MiscFlags = 0;
        p.stride = stride;

        // If this is an upload-once static buffer, make it IMMUTABLE
        if (bytes && size && p.desc.Usage == D3D11_USAGE_DEFAULT && p.desc.CPUAccessFlags == 0)
            p.desc.Usage = D3D11_USAGE_IMMUTABLE;

        const auto* b = static_cast<const uint8_t*>(bytes);
        p.data.assign(b, b + size);

        gfx.registry->RegisterBuffer(id, std::move(p));
    }
    else {
        // Ensure we don't lose SRV intent when reusing the id
        auto payload = gfx.registry->GetBuffer(id);
        const UINT merged = payload.desc.BindFlags | bindFlags;
        if (merged != payload.desc.BindFlags) {
            payload.desc.BindFlags = merged;
            gfx.registry->RegisterBuffer(id, std::move(payload));
        }
    }
    return id;
}


void LoadZone::load_entity_into_scene(TagHash tag, glm::quat quat, glm::vec4 pos)
{
    const SEntity entity = bin::parse<SEntity>(tag.data, tag.size);

    for (const auto& resource : entity.resources) {
        const auto ent_resource = bin::parse<SEntityResource>(resource.entity_resource.data,
            resource.entity_resource.size);
        if (ent_resource.resource18.type != 0x80806D8F) continue;

        const auto e = ent_resource.resource18.Parse<Unk_80806D8F>(resource.entity_resource);
        load_entity_model_into_scene(e.MeshFile, quat, pos, e.entity_material_map, e.materials);
    }
}

void LoadZone::load_entity_model_into_scene(TagHash sem,
    glm::quat quat,
    glm::vec4 pos,
    std::vector<Unk_808072C5> tech_maps,
    std::vector<TagHash> ext_techs)
{
    const SEntityModel model = bin::parse<SEntityModel>(sem.data, sem.size);

    RenderEntity re{};
    re.pos = pos;
    re.rot = quat;
    re.id = sem.hash;
    re.meshData = model;

    // ---- external materials (keep shared_ptr ownership) ----
    re.external_mats.clear();
    re.external_mats.reserve(ext_techs.size());
    for (const auto& mat : ext_techs) {
        // (Optional) your validation pass...
        // const auto technique = bin::parse<STechnique>(mat.data, mat.size, bin::Endian::Little);

        auto tech = gfx.assets->EnqueueTechnique(mat);      // returns AssetHandle<...>
        re.external_mats.push_back(tech.get());             // <-- if vector holds shared_ptr
        // If vector holds raw pointers, change the vector type to shared_ptr to avoid dangling.
    }

    // ---- meshes ----
    re.meshs.clear();
    re.meshs.reserve(model.parts.size());
    re.external_material_mapping = std::move(tech_maps);

    for (const auto& meshes : model.parts)
    {
        struct GroupRef {
            uint32_t v1Id{}, idxId{}, v2Id{}, colId{}, v3Id{}, v0Id{}, skinid{};
            uint32_t v0Stride{}, v1Stride{}, v2Stride{}, v3Stride{}, skinStride{}, colStride{};
            uint32_t indexCount{}; DXGI_FORMAT indexFormat{ DXGI_FORMAT_UNKNOWN }; bool idx32{};
        } gr{};

        DynamicMesh dm{};

        // ---- Register buffers once & derive metadata from headers (no registry lookups) ----
        if (meshes.index_buffer.hash != 0xFFFFFFFFu) {
            const auto ibh = bin::parse<IndexBufferHeader>(meshes.index_buffer.data, meshes.index_buffer.size, bin::Endian::Little);
            const void* ibBytes = TagHash(meshes.index_buffer.reference).data;
            gr.idxId = RegisterBufferBlob(ibBytes, ibh.dataSize, meshes.index_buffer.hash, D3D11_BIND_INDEX_BUFFER, 0);
            gr.idx32 = (ibh.is32 != 0);
            gr.indexFormat = gr.idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
            gr.indexCount = gr.idx32 ? (ibh.dataSize / 4u) : (ibh.dataSize / 2u);
        }
        auto regVB = [&](const auto& buf, uint32_t& idOut, uint32_t& strideOut, UINT extraBind = 0u)
            {
                if (buf.hash == 0xFFFFFFFFu) return;
                const auto vbh = bin::parse<VertexBufferHeader>(buf.data, buf.size, bin::Endian::Little);
                const void* vbBytes = TagHash(buf.reference).data;
                idOut = RegisterBufferBlob(vbBytes, vbh.dataSize, buf.hash,
                    D3D11_BIND_VERTEX_BUFFER | extraBind, vbh.stride);
                strideOut = vbh.stride;
            };
        regVB(meshes.vertex0_buffer, gr.v0Id, gr.v0Stride);
        regVB(meshes.vertex1_buffer, gr.v1Id, gr.v1Stride);
        regVB(meshes.buffer2, gr.v2Id, gr.v2Stride);
        regVB(meshes.buffer3, gr.v3Id, gr.v3Stride);
        regVB(meshes.skinning_buffer, gr.skinid, gr.skinStride);
        // Colour buffer needs SRV later ? add SRV bind up-front
        regVB(meshes.colour_buffer, gr.colId, gr.colStride, D3D11_BIND_SHADER_RESOURCE);

        // pipeline ranges
        dm.input_layout_per_render_stage = meshes.input_layout_per_render_stage;
        dm.part_range_per_render_stage = meshes.part_range_per_render_stage;

        // ---- Build BufferGroupDynamic ----
        auto grp = std::make_shared<BufferGroupDynamic>();

        if (gr.v0Id) { grp->vertex0_buffer = gfx.assets->EnqueueBuffer(gr.v0Id).future.get(); grp->vertex0Stride = gr.v0Stride; }
        if (gr.idxId) { grp->index_buffer = gfx.assets->EnqueueBuffer(gr.idxId).future.get(); grp->indexFormat = gr.indexFormat; grp->indexCount = gr.indexCount; }
        if (gr.v1Id) { grp->vertex1_buffer = gfx.assets->EnqueueBuffer(gr.v1Id).future.get(); grp->vertex1Stride = gr.v1Stride; }
        if (gr.v2Id) { grp->buffer2 = gfx.assets->EnqueueBuffer(gr.v2Id).future.get(); grp->buffer2Stride = gr.v2Stride; }
        if (gr.v3Id) { grp->buffer3 = gfx.assets->EnqueueBuffer(gr.v3Id).future.get(); grp->buffer3Stride = gr.v3Stride; }
        if (gr.skinid) { grp->skinning_buffer = gfx.assets->EnqueueBuffer(gr.skinid).future.get(); grp->skinningStride = gr.skinStride; }

        if (gr.colId) {
            BufferSRVMeta meta{};
            meta.typedFormat = (gr.colStride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
            meta.bytesPerElement = gr.colStride;

            auto res = gfx.assets->EnqueueBufferSRV(gr.colId, meta).future.get();   // returns BufferSRVRes with ComPtrs
            // store SRV without extra AddRef/Release churn
            grp->color = std::move(res->srv);
            grp->colorStride = gr.colStride;
        }

        // ---- DynamicMesh & parts ----
        auto meshPtr = std::make_shared<DynamicMesh>();
        meshPtr->buffers = grp;
        meshPtr->input_layout_per_render_stage = meshes.input_layout_per_render_stage;
        meshPtr->part_range_per_render_stage = meshes.part_range_per_render_stage;

        meshPtr->parts.reserve(meshes.parts.size());
        for (const auto& p : meshes.parts)
        {
            auto dmp = std::make_shared<DynamicMeshPart>();
            dmp->meshpartinfo = p;

            if (p.varient_shader_index == 65535) {
                auto tech = gfx.assets->EnqueueTechnique(p.technique);
                dmp->technique = tech.get();

                // Optional: expensive decode; keep if necessary
                if (p.technique.hash != 0xFFFFFFFFu) {
                    const auto technique = bin::parse<STechnique>(p.technique.data, p.technique.size, bin::Endian::Little);
                    const TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                        technique.PixelShader.TFX_Constants);
                    for (const auto ch : prog.channels) re.channels[ch] = 1.0f;
                }
            }
            else {
                dmp->techniqueId = 0;
                dmp->technique = nullptr;
            }

            meshPtr->parts.emplace_back(std::move(dmp));
        }

        re.meshs.emplace_back(std::move(meshPtr));
    }

    entities.emplace_back(std::move(re));
}
