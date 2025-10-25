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
    return id;
}

RenderStatic StaticRenderer::Build()
{
    printf("StaticRenderer::Build for static hash %08X\n", static_hash_.hash);

    // --- Parse model + mesh ---
    auto static_tag = TagHash(static_hash_);
    auto s = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);

    auto mesh_tag = TagHash(s.opaque_meshes);
    auto m = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);

    // Pick LOD (your logic)
    int max_detail = 0xff;
    for (auto group : m.mesh_groups) {
        if (group.TfxRenderStage < max_detail) {
            max_detail = group.TfxRenderStage;
        }
    }

    // ---- Collect buffer-group ids + index format once (avoid TagHash(ids[1]) later) ----
    struct GroupRef { uint32_t posId{}, idxId{}, uvId{}, colId{}; bool idx32{}; };
    std::vector<GroupRef> groupRefs;
    groupRefs.reserve(m.buffers.size());

    for (const auto& bg : m.buffers) {
        GroupRef gr{};
        if (bg.IndexBuffer.hash != 0xffffffff) {
            auto ibh = bin::parse<IndexBufferHeader>(bg.IndexBuffer.data, bg.IndexBuffer.size, bin::Endian::Little);
            auto ib_bytes = TagHash(bg.IndexBuffer.reference).data;
            gr.idxId = RegisterBufferBlob(ib_bytes, ibh.dataSize, bg.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
            gr.idx32 = (ibh.is32 != 0);
        }
        if (bg.VertexBuffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(bg.VertexBuffer.data, bg.VertexBuffer.size, bin::Endian::Little);
            auto vb_bytes = TagHash(bg.VertexBuffer.reference).data;
            gr.posId = RegisterBufferBlob(vb_bytes, vbh.dataSize, bg.VertexBuffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (bg.UVBuffer.hash != 0xffffffff) {
            auto uvbh = bin::parse<VertexBufferHeader>(bg.UVBuffer.data, bg.UVBuffer.size, bin::Endian::Little);
            auto uv_bytes = TagHash(bg.UVBuffer.reference).data;
            gr.uvId = RegisterBufferBlob(uv_bytes, uvbh.dataSize, bg.UVBuffer.hash, D3D11_BIND_VERTEX_BUFFER, uvbh.stride);
        }
        if (bg.VertexColourBuffer.hash != 0xffffffff) {
            auto vcbh = bin::parse<VertexBufferHeader>(bg.VertexColourBuffer.data, bg.VertexColourBuffer.size, bin::Endian::Little);
            auto vc_bytes = TagHash(bg.VertexColourBuffer.reference).data;
            gr.colId = RegisterBufferBlob(vc_bytes, vcbh.dataSize, bg.VertexColourBuffer.hash, D3D11_BIND_VERTEX_BUFFER, vcbh.stride);
        }
        groupRefs.push_back(gr);
    }

    UINT highestDetail = 99;
    for (const auto& party : m.parts) {
        if (party.LodCatagory < highestDetail) {
            highestDetail = party.LodCatagory;
        }
    }

    // ---- Collect per-part technique id + buffer group index (aligned vectors) ----

    std::vector<TagHash> techIds;
    std::vector<size_t>  partGroupIdx;
    std::vector<uint32_t> partInputLayoutIdx;

    int matIndex = 0;
    for (const auto& mg : m.mesh_groups) {
        if (mg.TfxRenderStage != max_detail) {
            ++matIndex; 
            continue;
        }
        if (m.parts[mg.part_index].LodCatagory != highestDetail) {
            ++matIndex;
            continue;
        }
        partGroupIdx.push_back(static_cast<size_t>(mg.part_index));     // which buffer group this part uses
        techIds.push_back(s.Techniques[matIndex].Unk0);                 // your technique tag
        partInputLayoutIdx.push_back(mg.input_layout_index);            // remember IL index if you want per-part IL
        ++matIndex;
    }

    // ---- Enqueue buffer groups (parallel) ----
    std::vector<std::shared_future<std::shared_ptr<BufferGroup>>> groupF;
    groupF.reserve(groupRefs.size());

    for (const auto& gr : groupRefs) {
        auto fut = gfx_.pool->Submit([this, gr]() {
            auto grp = std::make_shared<BufferGroup>();

            if (gr.posId) {
                grp->vertex = gfx_.assets->EnqueueBuffer(gr.posId).future.get();
                grp->vertexStride = gfx_.registry->GetBuffer(gr.posId).stride;
            }
            if (gr.idxId) {
                grp->index = gfx_.assets->EnqueueBuffer(gr.idxId).future.get();
                const UINT byteWidth = gfx_.registry->GetBuffer(gr.idxId).desc.ByteWidth;
                grp->indexFormat = gr.idx32 ? DXGI_FORMAT_R32_UINT : DXGI_FORMAT_R16_UINT;
                grp->indexCount = gr.idx32 ? (byteWidth / 4u) : (byteWidth / 2u);
            }
            if (gr.uvId) {
                grp->uv = gfx_.assets->EnqueueBuffer(gr.uvId).future.get();
                grp->uvStride = gfx_.registry->GetBuffer(gr.uvId).stride;
            }
            if (gr.colId) {
                const auto& buf = gfx_.registry->GetBuffer(gr.colId);
                const UINT stride = buf.stride;
                const UINT byteWidth = buf.desc.ByteWidth;
                BufferSRVMeta meta{};
                if (stride == 1) {
                    meta.typedFormat = DXGI_FORMAT_R8_UNORM;
                }
                else {
                    meta.typedFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
                }
                meta.bytesPerElement = stride;
                auto res = gfx_.assets->EnqueueBufferSRV(gr.colId, meta).future.get();

                ID3D11ShaderResourceView* raw = res->srv.Get();
                if (raw) raw->AddRef();  // take our own ref

                grp->color.reset(raw, [](ID3D11ShaderResourceView* p) { if (p) p->Release(); });
                grp->colorStride = gfx_.registry->GetBuffer(gr.colId).stride;               // ComPtr copy (AddRef)
            }
            return grp;
            }).share();
        groupF.push_back(fut);
    }

    std::vector<std::shared_future<std::shared_ptr<EntropyAssets::Technique>>> techF(techIds.size());
    for (size_t i = 0; i < techIds.size(); ++i) {
        const uint32_t tid32 = techIds[i].hash;
        printf("[Build] part %zu technique %08X\n", i, tid32);
        techF[i] = gfx_.assets->EnqueueTechnique(techIds[i]); // returns shared_future<shared_ptr<Technique>>
    }

    // ---- Assemble mesh ----
    auto mesh = std::make_shared<StaticMesh>();
    mesh->groups.reserve(groupF.size());
    for (auto& f : groupF) {
        try {
            mesh->groups.push_back(f.get());
        }
        catch (const std::exception& e) {
            printf("[Build][Group] failed: %s\n", e.what());
            mesh->groups.push_back(nullptr);
        }
    }


    mesh->parts.resize(techIds.size());
    for (size_t i = 0; i < partGroupIdx.size(); ++i) {
        std::shared_ptr<EntropyAssets::Technique> t;
        try {
            if (techF[i].valid()) t = techF[i].get();
        }
        catch (const std::exception& e) {
            printf("[Build][Tech] id=%08X get() failed: %s\n", techIds[i].hash, e.what());
        }
        mesh->parts[i].techniqueId = techIds[i].hash;
        mesh->parts[i].technique = std::move(t);
        mesh->parts[i].partInfo = m.parts[partGroupIdx[i]];
        mesh->input_layout_index = partInputLayoutIdx[i];
    }
    mesh->id = static_hash_.hash;

    // ---- Output renderable ----
    RenderStatic out{};
    out.mesh = std::move(mesh);
    out.meshData = m; // if you need original meta later
    return out;
}

