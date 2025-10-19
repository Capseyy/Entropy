// RenderStatic.h
#pragma once
#include <memory>
#include <DirectXMath.h>
#include "Runtime/Assets/StaticMesh.h"

struct RenderStatic {
    std::shared_ptr<StaticMesh> mesh;     // geometry + techniques (built by StaticRenderer)
    DirectX::XMMATRIX           world;    // per-object transform (if you have one)
    // add per-part overrides if your format has them
};
