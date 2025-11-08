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

        const uint8_t* b = static_cast<const uint8_t*>(bytes);
        p.data.assign(b, b + size);

        gfx.registry->RegisterBuffer(id, std::move(p));
    }
    return id;
}


void LoadZone::load_entity_into_scene(TagHash tag, glm::quat quat, glm::vec4 pos)
{
    SEntity entity = bin::parse<SEntity>(tag.data, tag.size);

    for (auto& resource : entity.resources)
    {
        auto ent_resource = bin::parse<SEntityResource>(resource.entity_resource.data,
            resource.entity_resource.size);
        if (ent_resource.resource18.type != 0x80806D8F)
            continue;

        auto e = ent_resource.resource18.Parse<Unk_80806D8F>(resource.entity_resource);
        load_entity_model_into_scene(e.MeshFile, quat, pos,
			e.entity_material_map, e.materials);
        
    }
}

void LoadZone::load_entity_model_into_scene(TagHash sem, glm::quat quat, glm::vec4 pos, std::vector<Unk_808072C5> tech_maps, std::vector<TagHash> ext_techs)
{
    SEntityModel model = bin::parse<SEntityModel>(sem.data, sem.size);

    RenderEntity re{};                           // value-init ? zeros

    re.pos = pos;
    re.rot = quat;
    re.id = sem.hash;
    re.meshData = model;

    // ---- MATERIALS: keep ownership (no .get()) ----
    re.external_mats.clear();
    re.external_mats.reserve(ext_techs.size());
    for (auto& mat : ext_techs) {
        auto tech = gfx.assets->EnqueueTechnique(mat);   // shared_ptr<Technique>
        re.external_mats.push_back(tech.get());                // store the shared_ptr
    }

    // ---- MESHES ----
    re.meshs.clear();
    re.meshs.reserve(model.parts.size());
    re.external_material_mapping = tech_maps;
    for (auto meshes : model.parts)
    {
        struct GroupRef { uint32_t v1Id{}, idxId{}, v2Id{}, colId{}, v3Id{}, v0Id{}, skinid{}; bool idx32{}; };
        GroupRef gr{};
        DynamicMesh dm{};

        // -------- Register buffers (once per mesh) --------
        if (meshes.index_buffer.hash != 0xffffffff) {
            auto ibh = bin::parse<IndexBufferHeader>(meshes.index_buffer.data, meshes.index_buffer.size, bin::Endian::Little);
            auto ibBytes = TagHash(meshes.index_buffer.reference).data;
            gr.idxId = RegisterBufferBlob(ibBytes, ibh.dataSize, meshes.index_buffer.hash, D3D11_BIND_INDEX_BUFFER, 0);
            gr.idx32 = (ibh.is32 != 0);
        }
        if (meshes.vertex0_buffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.vertex0_buffer.data, meshes.vertex0_buffer.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.vertex0_buffer.reference).data;
            gr.v0Id = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.vertex0_buffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (meshes.vertex1_buffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.vertex1_buffer.data, meshes.vertex1_buffer.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.vertex1_buffer.reference).data;
            gr.v1Id = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.vertex1_buffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (meshes.buffer2.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.buffer2.data, meshes.buffer2.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.buffer2.reference).data;
            gr.v2Id = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.buffer2.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (meshes.buffer3.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.buffer3.data, meshes.buffer3.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.buffer3.reference).data;
            gr.v3Id = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.buffer3.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (meshes.skinning_buffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.skinning_buffer.data, meshes.skinning_buffer.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.skinning_buffer.reference).data;
            gr.skinid = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.skinning_buffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (meshes.colour_buffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(meshes.colour_buffer.data, meshes.colour_buffer.size, bin::Endian::Little);
            auto vbBytes = TagHash(meshes.colour_buffer.reference).data;
            gr.colId = RegisterBufferBlob(vbBytes, vbh.dataSize, meshes.colour_buffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }

        // Pipeline metadata
        dm.input_layout_per_render_stage = meshes.input_layout_per_render_stage;
        dm.part_range_per_render_stage = meshes.part_range_per_render_stage;

        // -------- Build one BufferGroupDynamic for this mesh --------
        auto grp = std::make_shared<BufferGroupDynamic>();

        // slot 0: positions/tangents
        if (gr.v0Id) {
            grp->vertex0_buffer = gfx.assets->EnqueueBuffer(gr.v0Id).future.get();
            grp->vertex0Stride = gfx.registry->GetBuffer(gr.v0Id).stride;
        }

        // index buffer
        if (gr.idxId) {
            grp->index_buffer = gfx.assets->EnqueueBuffer(gr.idxId).future.get();
            const UINT bw = gfx.registry->GetBuffer(gr.idxId).desc.ByteWidth;
            grp->indexFormat = gr.idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
            grp->indexCount = gr.idx32 ? (bw / 4u) : (bw / 2u);
        }

        // slot 1: texcoords
        if (gr.v1Id) {
            grp->vertex1_buffer = gfx.assets->EnqueueBuffer(gr.v1Id).future.get();
            grp->vertex1Stride = gfx.registry->GetBuffer(gr.v1Id).stride;
        }

        if (gr.colId) {
            const auto& buf = gfx.registry->GetBuffer(gr.colId);
            BufferSRVMeta meta{};
            meta.typedFormat = (buf.stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
            meta.bytesPerElement = buf.stride;

            auto res = gfx.assets->EnqueueBufferSRV(gr.colId, meta).future.get();
            ID3D11ShaderResourceView* raw = res->srv.Get();
            if (raw) raw->AddRef();        // take our ref
            grp->color.reset(raw, [](ID3D11ShaderResourceView* p) { if (p) p->Release(); });
            grp->colorStride = buf.stride;
        }
        if (gr.v2Id) {
            grp->buffer2 = gfx.assets->EnqueueBuffer(gr.v2Id).future.get();
            grp->buffer2Stride = gfx.registry->GetBuffer(gr.v2Id).stride;
        }
        if (gr.v3Id) {
            grp->buffer3 = gfx.assets->EnqueueBuffer(gr.v3Id).future.get();
            grp->buffer3Stride = gfx.registry->GetBuffer(gr.v3Id).stride;
        }
        if (gr.skinid) {
            grp->skinning_buffer = gfx.assets->EnqueueBuffer(gr.skinid).future.get();
            grp->skinningStride = gfx.registry->GetBuffer(gr.skinid).stride;
        }



        // -------- Select highest LOD and create parts --------

        auto mesh = std::make_shared<DynamicMesh>();
        mesh->buffers = grp;
        mesh->input_layout_per_render_stage = meshes.input_layout_per_render_stage;
        mesh->part_range_per_render_stage = meshes.part_range_per_render_stage;

        mesh->parts.clear();
        mesh->parts.reserve(meshes.parts.size());

        for (const auto& p : meshes.parts)
        {
            auto dmp = std::make_shared<DynamicMeshPart>();
            dmp->meshpartinfo = p;

            if (p.varient_shader_index == 65535) {
                auto tech = gfx.assets->EnqueueTechnique(p.technique);   // shared_ptr<Technique>
                dmp->technique = tech.get();
                //dmp->technique = gfx.assets->EnqueueTechnique(p.technique).get();
            }
            else {
                dmp->techniqueId = 0;
                dmp->technique = nullptr;
            }

            mesh->parts.emplace_back(std::move(dmp));     // vector<DynamicMeshPart>
        }

        re.meshs.push_back(std::move(mesh));              // vector<shared_ptr<DynamicMesh>>
    }

    entities.push_back(std::move(re));
}