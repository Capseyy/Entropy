// RenderStatic.h
#pragma once
#include <memory>
#include <DirectXMath.h>
#include "Runtime/Assets/StaticMesh.h"
#include "TigerEngine/Map/static.h"  // SStaticMeshData


struct RenderStatic {
    std::shared_ptr<StaticMesh> mesh;     // geometry + techniques (built by StaticRenderer)
	SStaticMeshData meshData;          // raw mesh data (for LOD or other purposes)
    std::vector<SStaticInstanceTransform>  world;    // per-object transform (if you have one)
    // add per-part overrides if your format has them
};
