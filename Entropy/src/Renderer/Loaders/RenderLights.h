#pragma once
#include <memory>
#include <DirectXMath.h>
#include "TigerEngine/Map/map.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp> 

struct RenderLight {
    glm::mat4 light_matrix;
    glm::quat rot;
    glm::vec3 pos;
    float_t scale;
    std::shared_ptr<EntropyAssets::Technique> technique;
    uint32_t idx;
    uint32_t parent;
    Vec4 unk50;
    
};