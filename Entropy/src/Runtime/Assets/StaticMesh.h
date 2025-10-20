#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <cstdint>
#include "TigerEngine/Map/static.h"

// forward-declare to avoid circular includes; include "Technique.h" where you use it
namespace EntropyAssets { struct Technique; }

// One VAO-ish “group” of buffers used together when drawing
struct BufferGroup {
    // created by AssetSystem and wrapped as shared_ptr
    std::shared_ptr<ID3D11Buffer> vertex;
    std::shared_ptr<ID3D11Buffer> index;
    std::shared_ptr<ID3D11Buffer> uv;
    std::shared_ptr<ID3D11ShaderResourceView> color;

    // optional metadata for binding
    UINT vertexStride = 0;
    UINT uvStride = 0;
    UINT colorStride = 0;

    UINT        indexCount = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT; // or R16 if your data is 16-bit
};

// A draw “part” that uses a specific technique
struct StaticMeshPart {
    uint32_t techniqueId = 0;
    std::shared_ptr<EntropyAssets::Technique> technique; // filled by AssetSystem
    std::vector<uint32_t> bufferGroupIndices;            // which groups this part uses (optional)
    SStaticMeshPart partInfo;
};

// The full mesh = groups + parts
struct StaticMesh {
    uint32_t id = 0;
    std::vector<std::shared_ptr<BufferGroup>> groups;
    std::vector<StaticMeshPart> parts;
    uint16_t input_layout_index;
};
