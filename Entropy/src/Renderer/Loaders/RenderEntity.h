#pragma once
#include <memory>
#include <DirectXMath.h>
#include "TigerEngine/Map/map.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> 
#include "TigerEngine/Entity/entity.h"
#include "TigerEngine/Technique/Tfx/tfx_program.h"

struct DynamicMeshPart {
    uint32_t techniqueId = 0;
    std::shared_ptr<EntropyAssets::Technique> technique; // filled by AssetSystem         // which groups this part uses (optional)
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
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT; // or R16 if your data is 16-bit
    uint16_t input_layout_index;
};

struct DynamicMesh {
    std::shared_ptr<BufferGroupDynamic> buffers;
    std::array<uint16_t, 25> part_range_per_render_stage;
    std::array<uint8_t, 24> input_layout_per_render_stage;
    std::vector<std::shared_ptr<DynamicMeshPart>> parts;
    

};

struct RenderEntity {
    std::vector<std::shared_ptr<DynamicMesh>> meshs;     // geometry + techniques (built by StaticRenderer)
    SEntityModel meshData;          // raw mesh data (for LOD or other purposes)
    glm::quat rot;    // per-object transform (if you have one)
    glm::vec4 pos;
    std::vector<std::shared_ptr<EntropyAssets::Technique>> external_mats;
    uint32_t id;
    std::vector<Unk_808072C5> external_material_mapping;
    std::unordered_map<uint32_t, float_t> channels;
    std::optional<Aabb> occlusion_bounds;
    // add per-part overrides if your format has them
};