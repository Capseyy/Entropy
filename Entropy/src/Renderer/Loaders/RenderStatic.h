// RenderStatic.h
#pragma once
#include <memory>
#include <DirectXMath.h>
#include "Runtime/Assets/StaticMesh.h"
#include "TigerEngine/Map/static.h"  // SStaticMeshData
#include "Renderer/Graphics/Render/FrustumCulling.h"

struct StaticMeshConstants {
    std::array<float_t,3> mesh_offset{};
    float             mesh_scale = 1.0f;
    float_t texcoord_scale{};
    std::array<float_t, 2> texcoord_offset{};
    uint32_t          max_colour_index = 0;
};


struct RenderStatic {
    SStaticMeshData mesh;      
    std::vector<ObjectVectors>  world;  
    std::vector<StaticSpecial> specials;
	std::vector<Aabb>  bounds;      
	uint64_t AOID = 0;  
	std::vector<DirectX::XMFLOAT4X4> worldMatrices;
    std::vector<uint32_t> techniques;
	uint32_t id = 0;



    // add per-part overrides if your format has them
};
