#pragma once

#include <cstddef>
#include <DirectXMath.h>
using namespace DirectX;

// Minimal viewport stub: adapt to your real one.
struct Viewport {
    XMFLOAT2 size; // (width, height)
    // Must return the matrix that maps target pixel -> projective (clip) space
    XMMATRIX target_pixel_to_projective() const;
};

struct alignas(16) View
{
    // 0x00
    float      resolution_width;                 // 0x00
    float      resolution_height;                // 0x04
    float      _pad08[2] = {};                   // 0x08 .. 0x0F

    // 0x10
    XMFLOAT4   view_miscellaneous = {};          // 0x10  (x=maxDepthPreProj, y=isFirstPerson, ...)
    XMFLOAT4   position = {};           // 0x20
    XMFLOAT4   unk30 = {};           // 0x30

    // 0x40 .. (each XMFLOAT4X4 is 64 bytes)
    XMFLOAT4X4 world_to_camera;                  // 0x40   (V)
    XMFLOAT4X4 camera_to_projective;             // 0x80   (P)
    XMFLOAT4X4 camera_to_world;                  // 0xC0   (V^-1)
    XMFLOAT4X4 projective_to_camera;             // 0x100  (P^-1)
    XMFLOAT4X4 world_to_projective;              // 0x140  (VP)
    XMFLOAT4X4 projective_to_world;              // 0x180  (VP^-1)
    XMFLOAT4X4 target_pixel_to_world;            // 0x1C0
    XMFLOAT4X4 target_pixel_to_camera;           // 0x200
    XMFLOAT4X4 unk240;                           // 0x240  (unused)
    XMFLOAT4X4 tptow_no_proj_w;                  // 0x280
    XMFLOAT4X4 unk2c0;                           // 0x2C0  (unused)

    // --- helpers ---
    static inline XMMATRIX load(const XMFLOAT4X4& m) { return XMLoadFloat4x4(&m); }
    static inline void     store(XMFLOAT4X4& dst, FXMMATRIX m) { XMStoreFloat4x4(&dst, m); }

    // Zero translation (keep upper-left 3x3 and w)
    static inline XMMATRIX drop_translation(FXMMATRIX m) {
        XMMATRIX r = m;
        // row-major: translation lives in r[3].xyz
        r.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        return r;
    }

    // ===== port of Rust derive_matrices =====
    void derive_matrices_vs(const Viewport& viewport)
    {
        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        const XMMATRIX V = load(world_to_camera);
        const XMMATRIX P = load(camera_to_projective);

        // In your Rust: world_to_projective = P * V  (column-math).
        // If your HLSL uses row-math (mul(float4, M)), prefer VP = V * P instead.
        constexpr bool kRowMath = true;
        const XMMATRIX VP = kRowMath ? XMMatrixMultiply(V, P) : XMMatrixMultiply(P, V);

        const XMMATRIX CW = XMMatrixInverse(nullptr, V);   // camera_to_world
        const XMMATRIX PInv = XMMatrixInverse(nullptr, P); // projective_to_camera
        const XMMATRIX VPInv = XMMatrixInverse(nullptr, VP);

        store(camera_to_world, CW);
        store(world_to_projective, VP);
        store(projective_to_world, VPInv);
        store(projective_to_camera, PInv);

        const XMMATRIX TPtP = viewport.target_pixel_to_projective();
        const XMMATRIX TPtoC = XMMatrixMultiply(PInv, TPtP);  // target_pixel_to_camera
        const XMMATRIX TPtoW = XMMatrixMultiply(CW, TPtoC); // target_pixel_to_world
        store(target_pixel_to_camera, TPtoC);
        store(target_pixel_to_world, TPtoW);

        // position = camera_to_world.w_axis  (translation).
        // row-major: translation is row 3 (r[3].xyz,1).
        XMFLOAT4 posF;
        XMStoreFloat4(&posF, CW.r[3]);
        position = posF;

        // unk30 = Vec4::Z - world_to_projective.w_axis
        // Rust uses column w_axis; here use the 4th ROW (consistent with row-math).
        // If you truly need the 4th COLUMN instead, read VP column 3.
        XMFLOAT4 vpRow3F; XMStoreFloat4(&vpRow3F, VP.r[3]);
        XMFLOAT4 vecZ = { 0.f, 0.f, 1.f, 0.f };
        unk30 = XMFLOAT4{ vecZ.x - vpRow3F.x, vecZ.y - vpRow3F.y, vecZ.z - vpRow3F.z, vecZ.w - vpRow3F.w };

        // ptow_no_proj_w = (ctow with zeroed translation) * ptoc
        const XMMATRIX ctow_no_pos = drop_translation(CW);
        const XMMATRIX ptow_no_proj_wM = XMMatrixMultiply(ctow_no_pos, PInv);
        const XMMATRIX tptow_no_proj_wM = XMMatrixMultiply(ptow_no_proj_wM, TPtP);
        store(tptow_no_proj_w, tptow_no_proj_wM);
    }
    void derive_matrices_ps(const Viewport& viewport)
    {
        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        const XMMATRIX V = load(world_to_camera);
        const XMMATRIX P = load(camera_to_projective);

        // In your Rust: world_to_projective = P * V  (column-math).
        // If your HLSL uses row-math (mul(float4, M)), prefer VP = V * P instead.
        constexpr bool kRowMath = true;
        const XMMATRIX VP = kRowMath ? XMMatrixMultiply(V, P) : XMMatrixMultiply(P, V);

        const XMMATRIX CW = XMMatrixInverse(nullptr, V);   // camera_to_world
        const XMMATRIX PInv = XMMatrixInverse(nullptr, P); // projective_to_camera
        const XMMATRIX VPInv = XMMatrixInverse(nullptr, VP);

        store(camera_to_world, CW);
        store(world_to_projective, VP);
    }
};

// ---- layout guards ----
static_assert(offsetof(View, resolution_width) == 0x00, "offset");
static_assert(offsetof(View, resolution_height) == 0x04, "offset");
static_assert(offsetof(View, view_miscellaneous) == 0x10, "offset");
static_assert(offsetof(View, position) == 0x20, "offset");
static_assert(offsetof(View, unk30) == 0x30, "offset");
static_assert(offsetof(View, world_to_camera) == 0x40, "offset");
static_assert(offsetof(View, camera_to_projective) == 0x80, "offset");
static_assert(offsetof(View, camera_to_world) == 0xC0, "offset");
static_assert(offsetof(View, projective_to_camera) == 0x100, "offset");
static_assert(offsetof(View, world_to_projective) == 0x140, "offset");
static_assert(offsetof(View, projective_to_world) == 0x180, "offset");
static_assert(offsetof(View, target_pixel_to_world) == 0x1C0, "offset");
static_assert(offsetof(View, target_pixel_to_camera) == 0x200, "offset");
static_assert(offsetof(View, unk240) == 0x240, "offset");
static_assert(offsetof(View, tptow_no_proj_w) == 0x280, "offset");
static_assert(offsetof(View, unk2c0) == 0x2C0, "offset");

