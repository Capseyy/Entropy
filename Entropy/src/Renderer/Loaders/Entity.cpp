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
        if (bytes && size && p.desc.Usage == D3D11_USAGE_DEFAULT && p.desc.CPUAccessFlags == 0)
            p.desc.Usage = D3D11_USAGE_IMMUTABLE;

        const auto* b = static_cast<const uint8_t*>(bytes);
        p.data.assign(b, b + size);

        gfx.registry->RegisterBuffer(id, std::move(p));
    }
    else {
       
        auto payload = gfx.registry->GetBuffer(id);
        const UINT merged = payload.desc.BindFlags | bindFlags;
        if (merged != payload.desc.BindFlags) {
            payload.desc.BindFlags = merged;
            gfx.registry->RegisterBuffer(id, std::move(payload));
        }
    }
    return id;
}

void LoadZone::load_entity_into_scene(TagHash& tag, glm::quat quat, glm::vec4 pos, int recursion_depth, EntityType et)
{
    const uint32_t maxDepth = 10;

    if (recursion_depth >= int(maxDepth))
        return;

    // remaining depth budget (including this node)
    const uint32_t remainingDepth = maxDepth - uint32_t(recursion_depth);

    // Build/fetch cached expanded spawn list
    auto spawns = collect_entity_spawns(tag, remainingDepth, et);

    // Instantiate (multiple instances still work because this is per call)
    for (auto& s : spawns)
    {
        TagHash sem(s.sem_hash32);

        load_entity_model_into_scene(
            sem,
            quat,
            pos,
            s.tech_maps,
            s.ext_techs,
            s.occlusion_bounds,
            s.et
        );
    }
}

void LoadZone::load_entity_model_into_scene(TagHash sem,
    glm::quat quat,
    glm::vec4 pos,
    std::vector<Unk_808072C5> tech_maps,
    std::vector<uint32_t> ext_techs, std::optional<Aabb> occlustion_bounds,
    EntityType et)
{
    const SEntityModel model = bin::parse<SEntityModel>(sem.data, sem.size);
    if (sem.hash == 0x80E32C94) {
		int u = 1;
    }
    RenderEntity re{};
    re.external_mats = ext_techs;
    re.occlusion_bounds = occlustion_bounds;
    re.external_material_mapping = tech_maps;
    re.meshData = model;
    re.pos = pos;
    re.rot = quat;
    re.id = sem.hash;
	re.rtype = et;
    for (const auto& part_group : model.parts) {
        re.meshs.push_back(part_group);
        for (const auto& part : part_group.parts) {
            if (part.varient_shader_index == 0xFFFF) {
                if (part.technique.hash == 0xffffffff) { continue; }
                const auto technique = bin::parse<STechnique>(part.technique.data, part.technique.size, bin::Endian::Little);
                TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants, part.technique.hash);
                for (const auto ch : prog.channels) re.channels[ch] = 1.0f;
                prog = TfxProgram::FromBytecode(technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants, part.technique.hash);
                for (const auto ch : prog.channels) re.channels[ch] = 1.0f;
            }
            else {
                const auto tech_tag = TagHash(ext_techs[tech_maps[part.varient_shader_index].technique_start]);
                if (tech_tag.hash == 0xffffffff) { continue; }
                const auto technique = bin::parse<STechnique>(tech_tag.data, tech_tag.size, bin::Endian::Little);
                TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants, tech_tag.hash);
                for (const auto ch : prog.channels) re.channels[ch] = 1.0f;
                prog = TfxProgram::FromBytecode(technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants, tech_tag.hash);
                for (const auto ch : prog.channels) re.channels[ch] = 1.0f;
            }
        }
       
    }
    auto cb = UpdateCB1_Single(model.model_offset, model.model_scale, pos.w, model.texcoord_scale.x, model.texcoord_offset.x, model.texcoord_offset.y, quat, pos);
    re.cb1_single = cb;
    entities.emplace_back(std::move(re));
}
