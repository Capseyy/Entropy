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
    std::shared_ptr<StaticMesh> mesh;     // geometry + techniques (built by StaticRenderer)
    StaticMeshConstants meshData;          // raw mesh data (for LOD or other purposes)
    std::vector<ObjectVectors>  world;    // per-object transform (if you have one)
    Microsoft::WRL::ComPtr<ID3D11Buffer> cb1;
    std::vector<std::shared_ptr<StaticSpecial>> specials;
	std::vector<Aabb>  bounds;      // axis-aligned bounding box (in model space)
	uint64_t AOID = 0;  // AmbientOccusion ID
	std::vector<DirectX::XMFLOAT4X4> worldMatrices; // precomputed world matrices for faster rendering


    // add per-part overrides if your format has them
};
