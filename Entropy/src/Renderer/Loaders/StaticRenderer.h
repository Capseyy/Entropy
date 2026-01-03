#pragma once
#include <cstdint>
#include "RenderStatic.h"     
#include "TigerEngine/tag.h"

class Graphics;               

class StaticRenderer {
public:
    explicit StaticRenderer(const TagHash& tag) : static_hash_(tag) {}
    
    RenderStatic Build();

private:
    TagHash  static_hash_;
    uint32_t RegisterBufferBlob(const void* bytes, size_t size, uint32_t id,
        UINT bindFlags, UINT stride = 0);
};