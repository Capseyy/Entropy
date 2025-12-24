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
    const SEntity entity = bin::parse<SEntity>(tag.data, tag.size);

    for (const auto& resource : entity.resources) {

        const auto ent_resource =
            bin::parse<SEntityResource>(resource.entity_resource.data,
                resource.entity_resource.size);

        if (ent_resource.resource18.type == 0x80806D8F)
        {
            const auto e =
                ent_resource.resource18.Parse<Unk_80806D8F>(resource.entity_resource);

            load_entity_model_into_scene(
                e.MeshFile, quat, pos, e.entity_material_map, e.materials, {}, et);
        }
        else if (ent_resource.resource18.type == 0x80808179) {
            const auto e =
                ent_resource.resource18.Parse<Unk_80808179>(resource.entity_resource);
            for (const auto& unk_entry : e.unk10) {
                if (unk_entry.unk10.type == 0x808067B9) {
                    const auto e =
                        unk_entry.unk10.Parse<Unk_808067B9>(resource.entity_resource);
                    for (const auto& psys : e.particle_systems) {
                        if (psys.particle_system.success) {
                            const auto psystem =
                                bin::parse<SParticleSystem>(psys.particle_system.data,
                                psys.particle_system.size);
                            if (psystem.particle_mesh.success){
								const TagHash mesh_tag(psystem.particle_mesh.tagHash32);
                                const auto mesh_parent =
                                    bin::parse<Unk_80806929>(mesh_tag.data,
                                        mesh_tag.size);
								std::vector<uint32_t> ext_techs;
								ext_techs.push_back(psystem.technique_hash);
                                for (const auto& mesh_entry : mesh_parent.unk10) {
                              
                                    load_entity_model_into_scene(
                                        mesh_entry.sem, quat, pos, {}, ext_techs, {}, EntityType::ParticleSystem);
									}
                                }
							}
                    }
				}
            }
        }

        for (const auto& table_entry : ent_resource.relative_table) {
            auto WH = table_entry.Parse<WideHash>(resource.entity_resource);
            if (WH.success && WH.reference == 0x80809AD8) {
                if (recursion_depth >= 10) {
                    //printf("Max recursion depth reached when loading entity %08x\n", tag.hash);
                    return;
                }
                auto th = TagHash(WH.tagHash32);
                load_entity_into_scene(th, quat, pos, recursion_depth+=1, EntityType::ChildEntity);
            }
        }

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
