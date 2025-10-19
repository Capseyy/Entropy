#pragma once
#include <cstdint>
#include "RenderStatic.h"     // has StaticMesh etc.
#include "TigerEngine/tag.h"// TagHash

class Graphics;               // <- forward declare (do NOT include here)

class StaticRenderer {
public:
    StaticRenderer(Graphics& gfx, TagHash static_hash);
    RenderStatic Build();

private:
    Graphics& gfx_;           // <- this must exist in the header
    TagHash  static_hash_;

    uint32_t RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
        UINT bindFlags, UINT stride = 0);
};