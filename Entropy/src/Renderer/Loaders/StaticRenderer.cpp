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
    // --- Parse this static and its mesh ---
	printf("StaticRenderer::Build for static hash %08X\n", static_hash_.hash);
    auto static_tag = TagHash(static_hash_);
    auto s = bin::parse<SStaticModel>(static_tag.data, static_tag.size, bin::Endian::Little);

    auto mesh_tag = TagHash(s.opaque_meshes);
    auto m = bin::parse<SStaticMeshData>(mesh_tag.data, mesh_tag.size, bin::Endian::Little);

    int max_detail = 0xff;
    for (auto group : m.mesh_groups) {
        if (group.TfxRenderStage < max_detail) {
            max_detail = group.TfxRenderStage;
            //printf("INPUT LAYOUT %d\n", group.input_layout_index);
            auto input_layout = INPUT_LAYOUTS[group.input_layout_index];
            //printf("Input Layout with %zu elements\n", input_layout.elements.size());
        }
    }

    // --- Register buffers per group (de-dup by id) ---
    std::vector<std::array<uint32_t, 4>> groupIds; // {pos, idx, uv, col}
    groupIds.reserve(m.buffers.size());

    for (const auto& bg : m.buffers) {
        uint32_t posId = 0, idxId = 0, uvId = 0, colId = 0;  // defaults when a buffer is absent
        if (bg.IndexBuffer.hash != 0xffffffff) {
            auto ibh = bin::parse<IndexBufferHeader>(bg.IndexBuffer.data, bg.IndexBuffer.size, bin::Endian::Little);
            auto ib_buffer = TagHash(bg.IndexBuffer.reference).data;
            idxId = RegisterBufferBlob(ib_buffer, ibh.dataSize, bg.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
        }
        if (bg.VertexBuffer.hash != 0xffffffff) {
            auto vbh = bin::parse<VertexBufferHeader>(bg.VertexBuffer.data, bg.VertexBuffer.size, bin::Endian::Little);
            auto vb_buffer = TagHash(bg.VertexBuffer.reference).data;
            posId = RegisterBufferBlob(vb_buffer, vbh.dataSize, bg.VertexBuffer.hash, D3D11_BIND_VERTEX_BUFFER, vbh.stride);
        }
        if (bg.UVBuffer.hash != 0xffffffff) {
            auto uvbh = bin::parse<VertexBufferHeader>(bg.UVBuffer.data, bg.UVBuffer.size, bin::Endian::Little);
            auto uv_buffer = TagHash(bg.UVBuffer.reference).data;
            uvId = RegisterBufferBlob(uv_buffer, uvbh.dataSize, bg.UVBuffer.hash, D3D11_BIND_VERTEX_BUFFER, uvbh.stride);
        }
        if (bg.VertexColourBuffer.hash != 0xffffffff) {
            auto vcbh = bin::parse<VertexBufferHeader>(bg.VertexColourBuffer.data, bg.VertexColourBuffer.size, bin::Endian::Little);
            auto vc_buffer = TagHash(bg.VertexColourBuffer.reference).data;
            colId = RegisterBufferBlob(vc_buffer, vcbh.dataSize, bg.VertexColourBuffer.hash, D3D11_BIND_VERTEX_BUFFER, vcbh.stride);
        }
        groupIds.push_back({ posId, idxId, uvId, colId });
    }

    std::vector<TagHash> techIds;
    std::vector<size_t>   partGroupIdx;

    int MatIndex = 0;
    for (auto meshGroup : m.mesh_groups) {
        if (meshGroup.TfxRenderStage > max_detail) {
            MatIndex++;
            continue;
        }
		partGroupIdx.push_back(meshGroup.part_index);
        techIds.push_back(s.Techniques[MatIndex].Unk0);
		MatIndex++;
    }


    // --- Create GPU resources async + assemble mesh ---
    auto mesh = std::make_shared<StaticMesh>();

    std::vector<std::shared_future<std::shared_ptr<BufferGroup>>> groupF;
    groupF.reserve(groupIds.size());
    for (auto ids : groupIds) {
        auto f = gfx_.pool->Submit([this, ids] {
            auto grp = std::make_shared<BufferGroup>();

            if (ids[0]) {
                grp->vertex = gfx_.assets->EnqueueBuffer(ids[0]).future.get();
                grp->vertexStride = gfx_.registry->GetBuffer(ids[0]).stride;
            }
            if (ids[1]) {
				auto ibufferTag = TagHash(ids[1]);
                auto ibh = bin::parse<IndexBufferHeader>(ibufferTag.data, ibufferTag.size, bin::Endian::Little);
                grp->index = gfx_.assets->EnqueueBuffer(ids[1]).future.get();
                const UINT byteWidth = gfx_.registry->GetBuffer(ids[1]).desc.ByteWidth;
                if (ibh.is32 == 0) {
                    grp->indexFormat = DXGI_FORMAT_R16_UINT;
                    grp->indexCount = byteWidth / 2u;
                }
                else {
                    grp->indexFormat = DXGI_FORMAT_R32_UINT;
					grp->indexCount = byteWidth / 4u;
                }
                // Your engine uses R16 indices in RenderFrame; default to R16 here
               
                
                
            }
            if (ids[2]) {
                grp->uv = gfx_.assets->EnqueueBuffer(ids[2]).future.get();
                grp->uvStride = gfx_.registry->GetBuffer(ids[2]).stride;
            }
            if (ids[3]) {
                grp->color = gfx_.assets->EnqueueBuffer(ids[3]).future.get();
                grp->colorStride = gfx_.registry->GetBuffer(ids[3]).stride;
            }
            return grp;
            }).share();
        groupF.push_back(f);
    }

    // Build technique futures safely (1 per part)
    std::vector<std::shared_future<std::shared_ptr<EntropyAssets::Technique>>> techF;
    std::vector<uint8_t> techOk;
    techF.reserve(techIds.size());
    techOk.reserve(techIds.size());

    for (auto technique_tag : techIds) {
		auto& tid = technique_tag.hash;
        if (tid != 0 && !gfx_.registry->HasTechnique(tid)) {
            techF.push_back(gfx_.assets->EnqueueTechnique(technique_tag ));  // returns shared_future<shared_ptr<Technique>>
            techOk.push_back(1);
        }
        else {
            techF.emplace_back();   // default-constructed invalid future
            techOk.push_back(0);
            OutputDebugStringA(("Technique missing/unregistered id=" + std::to_string(tid) + "\n").c_str());
        }
    }


    for (auto& f : groupF) mesh->groups.push_back(f.get());
    mesh->parts.resize(techIds.size());
    for (size_t i = 0; i < techIds.size(); ++i) {
        mesh->parts[i].techniqueId = techIds[i].hash;
        
    }

    RenderStatic out{};
    out.mesh = std::move(mesh);
    out.world = XMMatrixIdentity();
	out.meshData = m;
    return out;
}
