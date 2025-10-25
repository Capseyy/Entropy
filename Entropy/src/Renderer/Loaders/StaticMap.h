#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "RenderStatic.h"
#include "TigerEngine/tag.h"    

class Graphics;  

class StaticMap {
public:
    explicit StaticMap(Graphics& gfx);     
    bool Initialize(uint32_t mapRootHash);
    void LoadAll_Statics();                

    const std::vector<RenderStatic>& Statics() const { return statics_; }
    std::vector<RenderStatic> GetRenderList();
private:
    Graphics& gfx_;                        
    uint64_t  rootHash_ = 0;
    std::vector<TagHash> staticTags;
    std::vector<RenderStatic> statics_;
};
