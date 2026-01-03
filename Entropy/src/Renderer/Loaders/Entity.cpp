#include "Renderer/Loaders/Map.h"
#include "Renderer/Graphics/Graphics.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/Technique.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Map/TigerBuffer.h"
#include "StaticRenderer.h"
#include "TigerEngine/tag.h"      
#include "TigerEngine/Map/static.h"
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

void LoadZone::load_entity_into_scene(TagHash& tag, glm::quat quat, glm::vec4 pos, int recursion_depth, EntityType et, uint64_t world_id, std::string name)
{
    uint32_t maxDepth = 2;
    if (et == EntityType::Combatant) {
        maxDepth = 2;
    }
    else {
		maxDepth = 5;
    }
    if (recursion_depth >= int(maxDepth))
        return;

    const uint32_t remainingDepth = maxDepth - uint32_t(recursion_depth);

    const uint64_t key = MakeEntityCacheKey(tag.hash, remainingDepth, et);

    if (auto it = entity_render_cache.find(key); it != entity_render_cache.end()) {
        for (auto& proto : it->second) {
            auto chIt = proto.channels.find(0xA7A7FE43);
            if (chIt != proto.channels.end()) {
                Vec4 a;
				a.x = pos.x;
				a.y = pos.y;
				a.z = pos.z;
				a.w = 1.0f;
                chIt->second = a;
            }
        }
        
        for (auto& proto : it->second) {
            if (proto.rtype != EntityType::ChildEntity && proto.rtype != EntityType::CombatantChild) {
                if (world_id != 0) {
                    auto ent_name = loaded_entity_names.find(world_id);
                    if (ent_name != loaded_entity_names.end()) {
                        proto.name = ent_name->second;
                    }
                }
                if (et == EntityType::Combatant && name != "") {
                    proto.name += " - " + name;
                }
            }
            
        }
        
        

        spawn_from_cached_prototypes(it->second, quat, pos);
        printf("Used cached entity prototypes for %08x with depth %d\n", tag.hash, remainingDepth);
        return;
    }
    if (et == EntityType::Combatant){
        printf("Caching entity prototypes for combatant %08x with depth %d\n", tag.hash, remainingDepth);
    }
    
    auto spawns = collect_entity_spawns(tag, remainingDepth, et);

    std::vector<RenderEntity> protos;
    protos.reserve(spawns.size());

    for (auto& s : spawns)
    {
        TagHash sem(s.sem_hash32);

        RenderEntity proto = build_render_entity_prototype(
            sem,
            s.tech_maps,
            s.ext_techs,
            s.occlusion_bounds,
            s.et,
            s.partical_technique
        );

        proto.pos = glm::vec4(0, 0, 0, 1);
        proto.rot = glm::quat(1, 0, 0, 0);
        proto.cb1_single = {};

        protos.emplace_back(std::move(proto));
    }

    auto [insIt, _] = entity_render_cache.emplace(key, std::move(protos));
    for (auto& proto : insIt->second) {
        auto chIt = proto.channels.find(0xA7A7FE43);  
        if (chIt != proto.channels.end()) {
            Vec4 a;
            a.x = pos.x;
            a.y = pos.y;
            a.z = pos.z;
            a.w = 1.0f;
            chIt->second = a;            
        }
    }
    for (auto& proto : insIt->second) {
        if (proto.rtype != EntityType::ChildEntity && proto.rtype != EntityType::CombatantChild) {
            if (world_id != 0) {
                auto ent_name = loaded_entity_names.find(world_id);
                if (ent_name != loaded_entity_names.end()) {
                    proto.name = ent_name->second;
                }
            }
            if (et == EntityType::Combatant && name != "") {
                proto.name += " - " + name;
            }
        }
        
	}
    
    spawn_from_cached_prototypes(insIt->second, quat, pos);
}

void LoadZone::load_entity_model_into_scene(TagHash sem,
    glm::quat quat,
    glm::vec4 pos,
    std::vector<Unk_808072C5> tech_maps,
    std::vector<uint32_t> ext_techs, std::optional<Aabb> occlustion_bounds,
    EntityType et,
    std::optional<uint32_t> particle_tech,
    std::string combat_name)
{
    const SEntityModel model = bin::parse<SEntityModel>(sem.data, sem.size);

    RenderEntity re{};
    re.external_mats = ext_techs;
    re.occlusion_bounds = occlustion_bounds;
    re.external_material_mapping = tech_maps;
    re.meshData = model;
    re.pos = pos;
    re.rot = quat;
    re.id = sem.hash;
	re.rtype = et;
    re.partical_technique = particle_tech;
    re.occlusion_bounds = Aabb::FromCenterExtents(model.model_offset+pos, model.model_scale*pos.w);
    for (const auto& part_group : model.parts) {
        re.meshs.push_back(part_group);
        for (const auto& part : part_group.parts) {
            if (part.varient_shader_index == 0xFFFF) {
                if (part.technique.hash == 0xffffffff) { continue; }
                const auto technique = bin::parse<STechnique>(part.technique.data, part.technique.size, bin::Endian::Little);
                TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants, part.technique.hash);
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
                prog = TfxProgram::FromBytecode(technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants, part.technique.hash);
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
            }
            else {
                const auto tech_tag = TagHash(ext_techs[tech_maps[part.varient_shader_index].technique_start]);
                if (tech_tag.hash == 0xffffffff) { continue; }
                const auto technique = bin::parse<STechnique>(tech_tag.data, tech_tag.size, bin::Endian::Little);
                TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants, tech_tag.hash);
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
                prog = TfxProgram::FromBytecode(technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants, tech_tag.hash);
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
            }
        }
        if (particle_tech && particle_tech != 0) {
            const auto tech_tag = TagHash(*particle_tech);
            const auto technique = bin::parse<STechnique>(tech_tag.data, tech_tag.size, bin::Endian::Little);
            TfxProgram prog = TfxProgram::FromBytecode(technique.PixelShader.TFX_Bytecode,
                technique.PixelShader.TFX_Constants, tech_tag.hash);
            for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
            prog = TfxProgram::FromBytecode(technique.VertexShader.TFX_Bytecode,
                technique.VertexShader.TFX_Constants, tech_tag.hash);
            for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
        }
       
    }
    auto cb = UpdateCB1_Single(model.model_offset, model.model_scale, pos.w, model.texcoord_scale.x, model.texcoord_offset.x, model.texcoord_offset.y, quat, pos);
    re.cb1_single = cb;
    entities.emplace_back(std::move(re));
}

void LoadZone::spawn_from_cached_prototypes(
    const std::vector<RenderEntity>& protos,
    const glm::quat& quat,
    const glm::vec4& pos)
{
    entities.reserve(entities.size() + protos.size());

    for (const auto& p : protos)
    {
        RenderEntity re = p; 

        re.pos = pos;
        re.rot = quat;
		re.base_placement_pos = pos;

       
        const auto& model = re.meshData;

        re.occlusion_bounds = Aabb::FromCenterExtents(
            model.model_offset + pos,
            model.model_scale * pos.w
        );

        re.cb1_single = UpdateCB1_Single(
            model.model_offset,
            model.model_scale,
            pos.w,
            model.texcoord_scale.x,
            model.texcoord_offset.x,
            model.texcoord_offset.y,
            quat,
            pos
        );

        entities.emplace_back(std::move(re));
    }
}

RenderEntity LoadZone::build_render_entity_prototype(
    TagHash sem,
    const std::vector<Unk_808072C5>& tech_maps,
    const std::vector<uint32_t>& ext_techs,
    const std::optional<Aabb>& occlusion_bounds,
    EntityType et,
    std::optional<uint32_t> particle_tech)
{
    const SEntityModel model = bin::parse<SEntityModel>(sem.data, sem.size);

    RenderEntity re{};
    re.external_mats = ext_techs;
    re.occlusion_bounds = occlusion_bounds; 
    re.external_material_mapping = tech_maps;
    re.meshData = model;
    re.id = sem.hash;
    re.rtype = et;
	re.partical_technique = particle_tech;

    re.meshs.clear();
    re.meshs.reserve(model.parts.size());

    for (const auto& part_group : model.parts) {
        re.meshs.push_back(part_group);

        for (const auto& part : part_group.parts) {
            if (part.varient_shader_index == 0xFFFF) {
                if (part.technique.hash == 0xffffffff) continue;

                const auto technique = bin::parse<STechnique>(
                    part.technique.data, part.technique.size, bin::Endian::Little
                );

                TfxProgram prog = TfxProgram::FromBytecode(
                    technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants,
                    part.technique.hash
                );
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);

                prog = TfxProgram::FromBytecode(
                    technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants,
                    part.technique.hash
                );
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
            }
            else {
                const uint16_t vi = part.varient_shader_index;
                if (vi >= tech_maps.size()) continue;

                const uint32_t techIndex = tech_maps[vi].technique_start;
                if (techIndex >= ext_techs.size()) continue;

                const auto tech_tag = TagHash(ext_techs[techIndex]);
                if (tech_tag.hash == 0xffffffff) continue;

                const auto technique = bin::parse<STechnique>(
                    tech_tag.data, tech_tag.size, bin::Endian::Little
                );

                TfxProgram prog = TfxProgram::FromBytecode(
                    technique.PixelShader.TFX_Bytecode,
                    technique.PixelShader.TFX_Constants,
                    tech_tag.hash
                );
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);

                prog = TfxProgram::FromBytecode(
                    technique.VertexShader.TFX_Bytecode,
                    technique.VertexShader.TFX_Constants,
                    tech_tag.hash
                );
                for (const auto ch : prog.channels) re.channels[ch] = Vec4::splat(1.0f);
            }
        }
    }


    return re;
}
