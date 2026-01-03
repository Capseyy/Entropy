#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <memory>
#include <vector>
#include <cstdint>
#include "TigerEngine/Map/static.h"


namespace EntropyAssets { struct Technique; }


struct BufferGroup {
    
    std::shared_ptr<ID3D11Buffer> vertex;
    std::shared_ptr<ID3D11Buffer> index;
    std::shared_ptr<ID3D11Buffer> uv;
    Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> color;

    
    UINT vertexStride = 0;
    UINT uvStride = 0;
    UINT colorStride = 0;

    UINT        indexCount = 0;
    DXGI_FORMAT indexFormat = DXGI_FORMAT_R32_UINT; 
};


struct StaticMeshPart {
    uint32_t techniqueId = 0;
    std::vector<uint32_t> bufferGroupIndices;            
    SStaticMeshPart partInfo;
};


struct StaticMesh {
    uint32_t id = 0;
    std::vector<BufferGroup> groups;
    std::vector<SStaticMeshPart> parts;
    std::vector<SStaticMeshGroup> meshGroups;
};

struct StaticSpecial {
    uint32_t id = 0;
    SStaticSpecial part;
    uint16_t input_layout_index;
    uint32_t techniqueId = 0;
};
