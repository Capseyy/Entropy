// StaticMap.h
#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "RenderStatic.h"
#include "TigerEngine/tag.h"    

class Graphics;  // fwd

class StaticMap {
public:
    explicit StaticMap(Graphics& gfx);      // ctor
    bool Initialize(uint32_t mapRootHash);
    void LoadAll_Statics();                 // (name whatever you want)

    const std::vector<RenderStatic>& Statics() const { return statics_; }
    std::vector<RenderStatic> GetRenderList(); // NEW
private:
    Graphics& gfx_;                         // <-- THIS must exist
    uint64_t  rootHash_ = 0;
    std::vector<TagHash> staticTags;
    std::vector<RenderStatic> statics_;
};
