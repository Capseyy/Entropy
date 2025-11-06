#pragma once

#include <cstddef>
#include <DirectXMath.h>
#include "d3d11.h"
#include "wrl/client.h"
using namespace DirectX;

// Minimal viewport stub: adapt to your real one.
struct Viewport {
    XMFLOAT2 size; // (width, height)
    // Must return the matrix that maps target pixel -> projective (clip) space
    XMMATRIX target_pixel_to_projective() const;
};

inline Microsoft::WRL::ComPtr<ID3D11Buffer> g_scopeView_b12;

struct alignas(16) View
{
    // 0x00
    float      resolution_width;                 // 0x00
    float      resolution_height;                // 0x04
    float      _pad08[2] = {};                   // 0x08 .. 0x0F

    // 0x10
    XMFLOAT4   view_miscellaneous = { 0.0f,1.0f,0.0f,0.0f };          // 0x10  (x=maxDepthPreProj, y=isFirstPerson, ...)
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

    static inline DirectX::XMVECTOR GetColumn(const DirectX::XMMATRIX& M, int c)
    {
        using namespace DirectX;
        return XMVectorSet(M.r[0].m128_f32[c], M.r[1].m128_f32[c], M.r[2].m128_f32[c], M.r[3].m128_f32[c]);
    }

    static inline DirectX::XMMATRIX SetColumn(DirectX::XMMATRIX M, int c, DirectX::XMVECTOR v)
    {
        using namespace DirectX;
        M.r[0].m128_f32[c] = XMVectorGetX(v);
        M.r[1].m128_f32[c] = XMVectorGetY(v);
        M.r[2].m128_f32[c] = XMVectorGetZ(v);
        M.r[3].m128_f32[c] = XMVectorGetW(v);
        return M;
    }

    // Zero translation (keep rotation+scale; column 3 = [0,0,0,1])
    static inline DirectX::XMMATRIX DropTranslationRow(const DirectX::XMMATRIX& M)
    {
        using namespace DirectX;
        XMMATRIX R = M;
        R.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        return R;
    }

    // Helpers: load/store row-major XMFLOAT4X4 with explicit transpose for HLSL column-major
    static inline DirectX::XMMATRIX load_rm(const DirectX::XMFLOAT4X4& m) {
        using namespace DirectX;
        // XMFLOAT4X4 is row-major layout; to get the true column-math matrix, transpose it on load.
        return XMMatrixTranspose(XMLoadFloat4x4(&m));
    }
    static inline void store_rm(DirectX::XMFLOAT4X4& dst, const DirectX::XMMATRIX& colM) {
        using namespace DirectX;
        // Store the transpose so that HLSL column-major sees the intended columns.
        XMStoreFloat4x4(&dst, XMMatrixTranspose(colM));
    }
    static inline DirectX::XMMATRIX drop_translation_col(const DirectX::XMMATRIX& M) {
        using namespace DirectX;
        // zero the translation column (column 3), keep affine bottom element
        XMMATRIX R = M;
        R.r[0].m128_f32[3] = 0.f;
        R.r[1].m128_f32[3] = 0.f;
        R.r[2].m128_f32[3] = 0.f;
        R.r[3].m128_f32[3] = 1.f;
        return R;
    }

    void derive_matrices_vs(const Viewport& viewport)
    {
        using namespace DirectX;

        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        // ---- Column-math on CPU ------------------------------------------------
        // Stored row-major on CPU, so transpose on load to get column matrices:
        const XMMATRIX V = load_rm(world_to_camera);        // world -> camera
        const XMMATRIX P = load_rm(camera_to_projective);   // camera -> projective

        // Column pipeline (HLSL default): clip = P * V * worldPos
        const XMMATRIX CW = XMMatrixInverse(nullptr, V);          // camera -> world
        const XMMATRIX WP = XMMatrixMultiply(P, V);               // world -> projective
        const XMMATRIX PC = XMMatrixInverse(nullptr, P);          // projective -> camera
        const XMMATRIX PW = XMMatrixInverse(nullptr, WP);         // projective -> world

        // Write back as row-major buffers (store transpose)
        store_rm(camera_to_world, CW);   // cb12 c4..c7
        store_rm(world_to_projective, WP);   // cb12 c0..c3
        store_rm(projective_to_camera, PC);   // (if you keep it in cb12)
        store_rm(projective_to_world, PW);   // (if you keep it in cb12)

        // target_pixel_to_camera  = PC * target_pixel_to_projective
        // target_pixel_to_world   = CW * target_pixel_to_camera
        const XMMATRIX TPtP = viewport.target_pixel_to_projective(); // already row-major math inside
        const XMMATRIX TPtP_col = XMMatrixTranspose(TPtP);            // use as column-math
        const XMMATRIX TPtC = XMMatrixMultiply(PC, TPtP_col);
        const XMMATRIX TPtW = XMMatrixMultiply(CW, TPtC);
        store_rm(target_pixel_to_camera, XMMatrixIdentity()); // if you have a field; otherwise omit
        store_rm(target_pixel_to_world, TPtW);

        // position = camera_to_world.w_axis (column 3 in column-math)
        {
            XMFLOAT4X4 cwRM; XMStoreFloat4x4(&cwRM, XMMatrixTranspose(CW)); // back to row-major
            position = cwRM._41 ? XMFLOAT4(cwRM._41, cwRM._42, cwRM._43, cwRM._44)
                : XMFLOAT4(CW.r[3].m128_f32[0], CW.r[3].m128_f32[1], CW.r[3].m128_f32[2], CW.r[3].m128_f32[3]);
        }

        // unk30 = Z - world_to_projective.w_axis (use column 3)
        {
            const XMVECTOR vecZ = XMVectorSet(0.f, 0.f, 1.f, 0.f);
            const XMVECTOR wpW = XMVectorSet(WP.r[0].m128_f32[3], WP.r[1].m128_f32[3],
                WP.r[2].m128_f32[3], WP.r[3].m128_f32[3]);
            const XMVECTOR u30 = XMVectorSubtract(vecZ, wpW);
            XMStoreFloat4(&unk30, u30);
        }

        // ptow_no_proj_w = (ctow with zeroed translation) * ptoc (all column-math)
        const XMMATRIX CW_noT = drop_translation_col(CW);
        const XMMATRIX PTOW_noProjW = XMMatrixMultiply(CW_noT, PC);
        const XMMATRIX TPTOW_noProjW = XMMatrixMultiply(PTOW_noProjW, TPtP_col);
        store_rm(tptow_no_proj_w, TPTOW_noProjW);

        // Finally the camera_to_projective block at the end of cb12
        store_rm(camera_to_projective, P);    // cb12 c11..c14

        // Anything else in cb12 (target, misc) stays as you already compute:
        // target (c8) and view_miscellaneous (c9) are simple float4s; write them directly.
    }
    void derive_matrices_ps(const Viewport& viewport)
    {
        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        const XMMATRIX V = load(world_to_camera);
        const XMMATRIX P = load(camera_to_projective);

        constexpr bool kRowMath = true;
        const XMMATRIX VP = kRowMath ? XMMatrixMultiply(V, P) : XMMatrixMultiply(P, V);

        const XMMATRIX CW = XMMatrixInverse(nullptr, V);   // camera_to_world
        const XMMATRIX PInv = XMMatrixInverse(nullptr, P); // projective_to_camera
        const XMMATRIX VPInv = XMMatrixInverse(nullptr, VP);

        store(camera_to_world, CW);
        store(world_to_projective, VP);
    }
};


struct alignas(16) ScopeViewCB12_VS
{
    DirectX::XMFLOAT4X4 world_to_projective;   // c0..c3
    DirectX::XMFLOAT4X4 camera_to_world;       // c4..c7
    DirectX::XMFLOAT4   target;                // c8
    DirectX::XMFLOAT4   view_miscellaneous;    // c9
    DirectX::XMFLOAT4   view_unk20;            // c10
    DirectX::XMFLOAT4X4 camera_to_projective;  // c11..c14
};


inline void CreateScopeViewCB12(ID3D11Device* dev)
{
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(ScopeViewCB12_VS);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ScopeViewCB12_VS zero{}; // start zeroed
    D3D11_SUBRESOURCE_DATA init{ &zero, 0, 0 };
    dev->CreateBuffer(&bd, &init, g_scopeView_b12.GetAddressOf());
}

inline void UploadScopeViewCB12_All(
    ID3D11DeviceContext* ctx,
    const View& view,
    float targetWidth,
    float targetHeight)
{
    using namespace DirectX;
    D3D11_MAPPED_SUBRESOURCE m{};
    ctx->Map(g_scopeView_b12.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);

    auto* cb = reinterpret_cast<ScopeViewCB12_VS*>(m.pData);
    cb->world_to_projective = view.world_to_projective;
    cb->camera_to_world = view.camera_to_world;
    cb->camera_to_projective = view.camera_to_projective;

    const float invW = targetWidth ? 1.0f / targetWidth : 0.0f;
    const float invH = targetHeight ? 1.0f / targetHeight : 0.0f;
    cb->target = { targetWidth, targetHeight, invW, invH };

    cb->view_miscellaneous = view.view_miscellaneous;               // keep your values
    cb->view_unk20 = { 4.15325f, 1.24929f, -1.49012e-8f, 1.0f };

    ctx->Unmap(g_scopeView_b12.Get(), 0);

    ID3D11Buffer* b = g_scopeView_b12.Get();
    //ctx->VSSetConstantBuffers(12, 1, &b);
    //ctx->PSSetConstantBuffers(12, 1, &b);
    //ctx->GSSetConstantBuffers(12, 1, &b); // if GS uses it
}

// Must match the HLSL constant buffer layout exactly (16-byte alignment)
struct VSConstants
{
    DirectX::XMFLOAT4 meshOffset_meshScale; // xyz = meshOffset, w = meshScale
    DirectX::XMFLOAT4 uvScale_uvOffset;     // x = uvScaleX, y = uvOffX, z = uvOffY, w = maxColorOrClamp
};
