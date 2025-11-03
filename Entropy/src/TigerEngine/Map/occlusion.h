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

struct alignas(16) Aabb {
    glm::vec3 min{ 0.0f };
    glm::vec3 max{ 0.0f };

    // Use functions instead of static data members to avoid macro & incomplete-type issues
    static const Aabb& Infinite() {
        static const Aabb v{
            glm::vec3(-std::numeric_limits<float>::infinity()),
            glm::vec3(std::numeric_limits<float>::infinity())
        };
        return v;
    }
    static const Aabb& Zero() {
        static const Aabb v{ glm::vec3(0.0f), glm::vec3(0.0f) };
        return v;
    }

    // Build from center & extents
    static inline Aabb from_center_extents(const glm::vec3& center, const glm::vec3& extents) {
        return Aabb{ center - extents, center + extents };
    }

    static inline Aabb from_obbs(const std::vector<std::pair<glm::mat4, Aabb>>& obbs) {
        std::vector<glm::vec3> pts;
        pts.reserve(obbs.size() * 8);
        for (const auto& [transform, aabb] : obbs) {
            for (const glm::vec3& c : aabb.corners()) {
                glm::vec4 h = transform * glm::vec4(c, 1.0f);
                if (h.w != 0.0f) h /= h.w;
                pts.emplace_back(h);
            }
        }
        return from_points(pts);
    }

    static inline Aabb from_projection_matrix(const glm::mat4& local_to_world) {
        static const glm::vec3 base[8] = {
            {-1.f,-1.f,-1.f}, {-1.f,-1.f, 1.f}, {-1.f, 1.f,-1.f}, {-1.f, 1.f, 1.f},
            { 1.f,-1.f,-1.f}, { 1.f,-1.f, 1.f}, { 1.f, 1.f,-1.f}, { 1.f, 1.f, 1.f}
        };
        std::vector<glm::vec3> pts;
        pts.reserve(8);
        for (glm::vec3 p : base) {
            glm::vec4 h = local_to_world * glm::vec4(p, 1.0f);
            if (h.w != 0.0f) h /= h.w;
            pts.emplace_back(h);
        }
        return from_points(pts);
    }

    inline bool contains_point_oriented(const glm::vec3& point, const glm::quat& orientation) const {
        const glm::mat4 S = glm::scale(glm::mat4(1.0f), extents());
        const glm::mat4 R = glm::mat4_cast(orientation);
        const glm::mat4 T = glm::translate(glm::mat4(1.0f), center());
        const glm::mat4 invM = glm::inverse(T * R * S);
        glm::vec4 hp = invM * glm::vec4(point, 1.0f);
        if (hp.w != 0.0f) hp /= hp.w;
        const glm::vec3 p(hp);
        return (p.x >= -1.0f && p.x <= 1.0f) &&
            (p.y >= -1.0f && p.y <= 1.0f) &&
            (p.z >= -1.0f && p.z <= 1.0f);
    }

    inline float volume() const {
        const glm::vec3 d = max - min;
        return d.x * d.y * d.z;
    }

    inline glm::vec3 center() const { return (min + max) * 0.5f; }
    inline glm::vec3 dimensions() const { return  max - min; }
    inline glm::vec3 extents() const { return (max - min) * 0.5f; }
    inline float     radius() const { return glm::length(extents()); }

    template <typename Range>
    static inline Aabb from_points(const Range& points) {
        glm::vec3 mn(std::numeric_limits<float>::infinity());
        glm::vec3 mx(-std::numeric_limits<float>::infinity());
        for (const glm::vec3& p : points) {
            mn = glm::min(mn, p);
            mx = glm::max(mx, p);
        }
        return Aabb{ mn, mx };
    }

    static inline Aabb from_points(std::initializer_list<glm::vec3> points) {
        return from_points<std::initializer_list<glm::vec3>>(points);
    }

    inline std::array<glm::vec3, 8> corners() const {
        return {
            glm::vec3(min.x, min.y, min.z),
            glm::vec3(min.x, min.y, max.z),
            glm::vec3(min.x, max.y, min.z),
            glm::vec3(min.x, max.y, max.z),
            glm::vec3(max.x, min.y, min.z),
            glm::vec3(max.x, min.y, max.z),
            glm::vec3(max.x, max.y, min.z),
            glm::vec3(max.x, max.y, max.z),
        };
    }

    inline Aabb untransform(const glm::mat4& transform) const {
        const glm::mat4 inv = glm::inverse(transform);
        const auto c = corners();
        std::vector<glm::vec3> pts;
        pts.reserve(8);
        for (const auto& corner : c) {
            glm::vec4 h = inv * glm::vec4(corner, 1.0f);
            if (h.w != 0.0f) h /= h.w;
            pts.emplace_back(h);
        }
        return from_points(pts);
    }
};
