#pragma once
#define GLM_ENABLE_EXPERIMENTAL
#include <array>
#include <cstdint>
#include <vector>
#include <limits>
#include <algorithm>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/quaternion.hpp>
#include "Renderer/Graphics/Render/FrustumCulling.h"
struct ObjectVectors {
    glm::quat rotation;
    glm::vec3 translation;
    float     scale;        // uniform
};


struct SObjectOcclusionBounds {
    Aabb unk0;
    std::array<uint32_t, 4> unk10;
};


struct SOcclusionBounds {
    uint64_t fileSize;
	std::vector<SObjectOcclusionBounds> bounds;
};