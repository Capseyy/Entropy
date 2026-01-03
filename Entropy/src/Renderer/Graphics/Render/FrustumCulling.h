#pragma once
#if !defined(__HLSL_VERSION)  

#include <DirectXMath.h>
#include <algorithm>
#include <cmath>
#include <cfloat>
#include <cstdio>
#include <cstdarg>
#include <glm/glm.hpp>

using namespace DirectX;


struct Aabb {
    XMFLOAT3 minv{ 0,0,0 };
    XMFLOAT3 maxv{ 0,0,0 };

    XMFLOAT3 center()  const {
        return { (minv.x + maxv.x) * 0.5f,
                (minv.y + maxv.y) * 0.5f,
                (minv.z + maxv.z) * 0.5f };
    }
    XMFLOAT3 extents() const {
        return { (maxv.x - minv.x) * 0.5f,
                (maxv.y - minv.y) * 0.5f,
                (maxv.z - minv.z) * 0.5f };
    }
    static Aabb FromCenterExtents(const glm::vec4& center, const glm::vec4& extents) {
        return Aabb{
            XMFLOAT3{ center.x - extents.x, center.y - extents.y, center.z - extents.z },
            XMFLOAT3{ center.x + extents.x, center.y + extents.y, center.z + extents.z }
        };
    }
};


namespace culldbg {

    
    inline void print(const char* fmt, ...) {
        va_list ap; va_start(ap, fmt);
        vprintf(fmt, ap);
        va_end(ap);
    }

    inline const char* PlaneName(int i) {
        static const char* names[5] = { "Left","Right","Top","Bottom","Near" };
        return (i >= 0 && i < 5) ? names[i] : "?";
    }

    inline void printMat(const char* name, const XMFLOAT4X4& m) {
        print("%s\n", name);
        print("[ % .6f % .6f % .6f % .6f ]\n", m._11, m._12, m._13, m._14);
        print("[ % .6f % .6f % .6f % .6f ]\n", m._21, m._22, m._23, m._24);
        print("[ % .6f % .6f % .6f % .6f ]\n", m._31, m._32, m._33, m._34);
        print("[ % .6f % .6f % .6f % .6f ]\n", m._41, m._42, m._43, m._44);
    }

    inline void printVec3(const char* name, const DirectX::XMFLOAT3& v) {
        print("%s=(%.6f, %.6f, %.6f)", name, v.x, v.y, v.z);
    }

    inline void printAabb(const char* name, const Aabb& b) {
        print("%s: ", name);
        printVec3("min", b.minv); print(", ");
        printVec3("max", b.maxv); print("\n");

        const auto c = b.center();
        const auto e = b.extents();
        print("  center/extents/r: ");
        printVec3("c", c); print(", ");
        printVec3("e", e); print(", r=%.6f\n",
            std::sqrt(e.x * e.x + e.y * e.y + e.z * e.z));
    }
    
    struct Plane {
        XMFLOAT3 n{ 0,0,0 };
        float    d{ 0 };

        Plane normalize() const {
            const float len = std::sqrt(n.x * n.x + n.y * n.y + n.z * n.z);
            if (len <= 1e-20f) return *this;
            const float inv = 1.0f / len;
            return { {n.x * inv, n.y * inv, n.z * inv}, d * inv };
        }

        float distance(const XMFLOAT3& p) const {
            return n.x * p.x + n.y * p.y + n.z * p.z + d;
        }
    };

    inline void printPlane(const char* name, const Plane& p) {
        print("%s n=(% .6f,% .6f,% .6f) d=% .6f\n", name, p.n.x, p.n.y, p.n.z, p.d);
    }

    
    struct Frustum {
        Plane L, R, T, B, N; 

    
        static Frustum FromColumnMajor(const XMFLOAT4X4& M, bool near_is_c4_plus_c3)
        {
            
            const XMFLOAT4 c1{ M._11, M._21, M._31, M._41 };
            const XMFLOAT4 c2{ M._12, M._22, M._32, M._42 };
            const XMFLOAT4 c3{ M._13, M._23, M._33, M._43 };
            const XMFLOAT4 c4{ M._14, M._24, M._34, M._44 };

            auto add = [](const XMFLOAT4& a, const XMFLOAT4& b) { return XMFLOAT4{ a.x + b.x,a.y + b.y,a.z + b.z,a.w + b.w }; };
            auto sub = [](const XMFLOAT4& a, const XMFLOAT4& b) { return XMFLOAT4{ a.x - b.x,a.y - b.y,a.z - b.z,a.w - b.w }; };
            auto toP = [](const XMFLOAT4& v) { return Plane{ {v.x,v.y,v.z}, v.w }.normalize(); };

            Frustum f;
            f.L = toP(add(c4, c1));
            f.R = toP(sub(c4, c1));
            f.B = toP(add(c4, c2));
            f.T = toP(sub(c4, c2));
            f.N = toP(near_is_c4_plus_c3 ? add(c4, c3) : sub(c4, c3));
            return f;
        }

        static Frustum FromRowMajor(FXMMATRIX VP, bool near_is_r4_minus_r3)
        {
            XMFLOAT4X4 m; XMStoreFloat4x4(&m, VP);
            const XMVECTOR r1 = XMVectorSet(m._11, m._12, m._13, m._14);
            const XMVECTOR r2 = XMVectorSet(m._21, m._22, m._23, m._24);
            const XMVECTOR r3 = XMVectorSet(m._31, m._32, m._33, m._34);
            const XMVECTOR r4 = XMVectorSet(m._41, m._42, m._43, m._44);

            auto toP = [](FXMVECTOR v) {
                XMFLOAT4 a; XMStoreFloat4(&a, v);
                return Plane{ {a.x,a.y,a.z}, a.w }.normalize();
                };

            Frustum f;
            f.L = toP(r4 + r1);
            f.R = toP(r4 - r1);
            f.B = toP(r4 + r2);
            f.T = toP(r4 - r2);
            f.N = toP(near_is_r4_minus_r3 ? (r4 - r3) : r3);
            return f;
        }
    };

    inline void printFrustum(const Frustum& f) {
        printPlane("Left ", f.L);
        printPlane("Right", f.R);
        printPlane("Top  ", f.T);
        printPlane("Bottom", f.B);
        printPlane("Near ", f.N);
    }

    inline bool aabb_in_frustum_dbg(const Frustum& f,
        const Aabb& b,
        int* outFailedPlane = nullptr,
        bool verbose = false)
    {
        const Plane P[5] = { f.L, f.R, f.T, f.B, f.N };

        const XMFLOAT3 c = b.center();
        const XMFLOAT3 e = b.extents();
        const float eps = std::clamp(0.001f * (e.x + e.y + e.z), 0.05f, 5.0f);

        for (int i = 0; i < 5; ++i) {
            const Plane& pl = P[i];

            const float dist = pl.n.x * c.x + pl.n.y * c.y + pl.n.z * c.z + pl.d;
            const float r = std::fabs(pl.n.x) * e.x + std::fabs(pl.n.y) * e.y + std::fabs(pl.n.z) * e.z;

            if (dist < -r - eps) {
                if (outFailedPlane) *outFailedPlane = i;
                if (verbose) {
                    print("[Cull] AABB rejected by %s: dist=%.3f, r=%.3f, eps=%.3f\n"
                        "       plane n=(%.4f, %.4f, %.4f), d=%.4f\n"
                        "       center=(%.3f, %.3f, %.3f), extents=(%.3f, %.3f, %.3f)\n",
                        PlaneName(i), dist, r, eps,
                        pl.n.x, pl.n.y, pl.n.z, pl.d,
                        c.x, c.y, c.z, e.x, e.y, e.z);
                }
                return false;
            }
        }
        return true;
    }

    inline bool aabb_in_frustum(const Frustum& f, const Aabb& b) {
        return aabb_in_frustum_dbg(f, b, nullptr, false);
    }

}

#endif 
