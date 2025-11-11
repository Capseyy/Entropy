#pragma once
#include <cstdint>
#include <memory>
#include <vector>
#include "RenderStatic.h"
#include "TigerEngine/tag.h"    

class Graphics;  

class StaticMap {
public:    
    bool Initialize(uint32_t mapRootHash);
    void LoadAll_Statics();                

    const std::vector<RenderStatic>& Statics() const { return statics_; }
    const std::vector<RenderStatic>& GetRenderList() const;
private:                       
    uint64_t  rootHash_ = 0;
    std::vector<TagHash> staticTags;
    std::vector<RenderStatic> statics_;
};
