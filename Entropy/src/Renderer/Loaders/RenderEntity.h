#pragma once
#include <memory>
#include <DirectXMath.h>
#include "TigerEngine/Map/map.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> 
#include "TigerEngine/Entity/entity.h"
#include "TigerEngine/Technique/Tfx/tfx_program.h"
#include "Renderer/Graphics/Scope/instance.h"

enum class EntityType : uint8_t {
    Standard = 1,
    Activity =2,
    ParticleSystem = 3,
    Combatant = 4,
    SkyEntity = 5,
    ChildEntity = 6,
    CombatantChild =7,
};


static uint32_t g_entityTypeVisibleMask =
(1u << (uint32_t)EntityType::Standard) |
(1u << (uint32_t)EntityType::Activity) |
(1u << (uint32_t)EntityType::ParticleSystem) |
(1u << (uint32_t)EntityType::Combatant) |
(1u << (uint32_t)EntityType::SkyEntity) |
(1u << (uint32_t)EntityType::ChildEntity)|
(1u << (uint32_t)EntityType::CombatantChild);

static inline bool IsEntityTypeVisible(EntityType t)
{
    const uint32_t bit = 1u << (uint32_t)t;
    return (g_entityTypeVisibleMask & bit) != 0;
}

struct DynamicMeshPart {
    uint32_t techniqueId = 0;
    std::shared_ptr<EntropyAssets::Technique> technique;
    SDynamicMeshPart meshpartinfo;
};

struct BufferGroupDynamic {
    std::shared_ptr<ID3D11Buffer> vertex0_buffer;
    std::shared_ptr<ID3D11Buffer> vertex1_buffer;
    std::shared_ptr<ID3D11Buffer> buffer2;
    std::shared_ptr<ID3D11Buffer> buffer3;
    std::shared_ptr<ID3D11Buffer> index_buffer;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color;
    std::shared_ptr<ID3D11Buffer> skinning_buffer;
    std::vector<DynamicMeshPart> parts;
    UINT vertex0Stride = 0;
    UINT vertex1Stride = 0;
    UINT buffer2Stride = 0;
    UINT buffer3Stride = 0;
    UINT colorStride = 0;
    UINT skinningStride = 0;
    UINT        indexCount = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT; 
    uint16_t input_layout_index;

};

struct RenderEntity {
    std::vector<SDynamicMesh> meshs;    
    SEntityModel meshData;      
    glm::quat rot;    
    glm::vec4 pos;
    std::vector<uint32_t> external_mats;
    uint32_t id;
    std::vector<Unk_808072C5> external_material_mapping;
    std::unordered_map<uint32_t, Vec4> channels;
    std::optional<Aabb> occlusion_bounds;
    CB1Payload_override cb1_single;
	EntityType rtype = EntityType::Standard;
    std::optional<uint32_t> partical_technique;
	glm::vec4 base_placement_pos;
	std::string name;
};