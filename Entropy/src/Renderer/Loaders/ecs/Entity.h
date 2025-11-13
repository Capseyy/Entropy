#pragma once
#include <d3d11.h>
#include <shared_mutex>
#include "Runtime/Assets/AssetSystem.h"
#include "TigerEngine/Entity/entity.h"

enum class DynamicBufKind {
    Index,
    Vertex0,
    Vertex1,
    Buffer2,
    Buffer3,
    Color,
    Skin
};


static inline TagHash GetEntityBufTag(const SDynamicMesh& m, DynamicBufKind which)
{
    switch (which) {
    case DynamicBufKind::Index:   return m.index_buffer;
    case DynamicBufKind::Vertex0: return m.vertex0_buffer;
    case DynamicBufKind::Vertex1: return m.vertex1_buffer;
    case DynamicBufKind::Buffer2: return m.buffer2;
    case DynamicBufKind::Buffer3: return m.buffer3;
    case DynamicBufKind::Color:   return m.colour_buffer;
    case DynamicBufKind::Skin:    return m.skinning_buffer;
    }
    return {};
}

struct ResolvedEntityPart {
    // GPU objects (resolved once)
    std::shared_ptr<ID3D11Buffer> ib;
    std::shared_ptr<ID3D11Buffer> vb0;  UINT stride0 = 0; // vertex0
    std::shared_ptr<ID3D11Buffer> vb1;  UINT stride1 = 0; // vertex1
    std::shared_ptr<ID3D11Buffer> vb2;  UINT stride2 = 0; // buffer2
    std::shared_ptr<ID3D11Buffer> vb3;  UINT stride3 = 0; // buffer3
    std::shared_ptr<ID3D11Buffer> vSkin; UINT strideSkin = 0; // skin (optional)

    std::shared_ptr<EntropyAssets::BufferSRVRes> vCol; // color SRV (optional)
    UINT colorStride = 0;
    DXGI_FORMAT colorTypedFmt = DXGI_FORMAT_UNKNOWN;

    DXGI_FORMAT idxFmt = DXGI_FORMAT_R16_UINT;
    UINT indexCount = 0;
    UINT indexStart = 0;

    bool ready = false;
};

