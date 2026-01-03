#pragma once
#include <memory>
#include <DirectXMath.h>
#include "TigerEngine/Map/map.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> 
#include "TigerEngine/Technique/Tfx/tfx_program.h"
#include "Renderer/Graphics/Scope/instance.h"

struct alignas(16) TerrainCB64
{
    float mesh_offset[4];          
    float texcoord_offset[4];      

    
    float _zero0[4];               
    float _zero1[4];               
};

static_assert(sizeof(TerrainCB64) == 64, "TerrainCB64 must be exactly 64 bytes");
static_assert(alignof(TerrainCB64) == 16, "TerrainCB64 must be 16-byte aligned");


inline TerrainCB64 MakeTerrainCB64(float offX, float offY, float offZ, float offW,
    float t0, float t1, float t2, float t3)
{
    TerrainCB64 cb{};
    cb.mesh_offset[0] = offX;
    cb.mesh_offset[1] = offY;
    cb.mesh_offset[2] = offZ;
    cb.mesh_offset[3] = offW;

    cb.texcoord_offset[0] = t0;
    cb.texcoord_offset[1] = t1;
    cb.texcoord_offset[2] = t2;
    cb.texcoord_offset[3] = t3;

    
    

    return cb;
}

struct BufferGroupTerrain {
    std::shared_ptr<ID3D11Buffer> vertex0_buffer;
    std::shared_ptr<ID3D11Buffer> vertex1_buffer;
    std::shared_ptr<ID3D11Buffer> index_buffer;
    std::vector<STerrainPart> parts;
    UINT vertex0Stride = 0;
    UINT vertex1Stride = 0;
    UINT        indexCount = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT;
    uint16_t input_layout_index;

};

struct RenderTerrain {
    std::vector<SDynamicMesh> meshs;
    STerrain meshData;
    glm::quat rot;
    glm::vec4 pos;
    uint32_t id;
    std::optional<Aabb> occlusion_bounds;
    CB1Payload_override cb1_single;
};