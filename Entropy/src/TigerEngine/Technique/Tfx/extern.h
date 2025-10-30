#pragma once
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "tfx_runtime.h"
#include <DirectXMath.h>
#include <d3d11.h>
#undef min
#undef max
#include "Renderer/Graphics/Scope/view.h"

inline XMMATRIX XM(const Mat4& m) { return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m)); }
inline Mat4     M4(FXMMATRIX m) { Mat4 out; XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), m); return out; }

// --- tiny helpers for XMFLOAT4X4 ---
inline XMMATRIX      ToXM(const XMFLOAT4X4& m) { return XMLoadFloat4x4(&m); }
inline XMFLOAT4X4    FromXM(FXMMATRIX m) { XMFLOAT4X4 r; XMStoreFloat4x4(&r, m); return r; }

inline XMFLOAT4X4 MIdentity() { return FromXM(XMMatrixIdentity()); }
inline XMFLOAT4X4 MMul(const XMFLOAT4X4& a, const XMFLOAT4X4& b)
{
    return FromXM(XMMatrixMultiply(ToXM(a), ToXM(b)));
}

inline XMFLOAT4X4 MInverse(const XMFLOAT4X4& m)
{
    XMVECTOR det; return FromXM(XMMatrixInverse(&det, ToXM(m)));
}

// translation (row-major: last ROW)
inline XMFLOAT4 WAxis(const XMFLOAT4X4& m) { return { m._41,m._42,m._43,m._44 }; }
inline XMFLOAT4 VecZ() { return { 0,0,1,0 }; }

inline XMFLOAT4X4 RemoveTranslation(const XMFLOAT4X4& m)
{
    XMFLOAT4X4 r = m; r._41 = r._42 = r._43 = 0.f; r._44 = 1.f; return r;
}

// pixel-center -> clip/projective (D3D top-left origin)
inline XMFLOAT4X4 TargetPixelToProjective(float W, float H)
{
    const float sx = 2.f / W, sy = -2.f / H;
    const float ox = sx * 0.5f - 1.f;
    const float oy = 1.f - sy * 0.5f;

    XMFLOAT4X4 m = MIdentity();
    m._11 = sx; m._22 = sy;
    m._41 = ox; m._42 = oy; // translation
    return m;
}

// Inverse
inline Mat4 inverse(const Mat4& m) {
    XMVECTOR det{};
    return M4(XMMatrixInverse(&det, XM(m)));
}

// Multiply
inline Mat4 mul(const Mat4& a, const Mat4& b) { return M4(XMMatrixMultiply(XM(a), XM(b))); }

// Camera position / W-axis.
// ROW-MAJOR (DirectX-style): translation is in the LAST ROW (_41,_42,_43,_44)
inline Vec4 w_axis_rowmajor(const Mat4& m) {
    const float* p = reinterpret_cast<const float*>(&m);
    return Vec4(p[12], p[13], p[14], p[15]); // _41,_42,_43,_44
}
// COLUMN-MAJOR alternative (uncomment if your Mat4 is column-major):
// inline Vec4 w_axis_colmajor(const Mat4& m) {
//     const float* p = reinterpret_cast<const float*>(&m);
//     return Vec4(p[3], p[7], p[11], p[15]); // last COLUMN
// }

inline Vec4 vec4_Z() { return Vec4(0.f, 0.f, 1.f, 0.f); }

// Remove translation from a matrix (keep 3x3 rotation; w becomes [0,0,0,1])
inline Mat4 remove_translation_rowmajor(Mat4 m) {
    float* p = reinterpret_cast<float*>(&m);
    p[12] = p[13] = p[14] = 0.f; // zero _41,_42,_43
    p[15] = 1.f;                 // _44
    return m;
}

// Build matrix that maps pixel centers -> clip/projective space.
// x_ndc = ( (x+0.5)/W )*2 - 1 ; y_ndc = 1 - ( (y+0.5)/H )*2  (D3D top-left origin)
inline Mat4 target_pixel_to_projective(float W, float H) {
    const float sx = 2.0f / W;
    const float sy = -2.0f / H;
    const float ox = sx * 0.5f - 1.0f;
    const float oy = 1.0f - sy * 0.5f;

    Mat4 m = Mat4::identity();
    float* p = reinterpret_cast<float*>(&m);
    // row-major 4x4:
    p[0] = sx;  p[1] = 0;   p[2] = 0;  p[3] = 0;
    p[4] = 0;   p[5] = sy;  p[6] = 0;  p[7] = 0;
    p[8] = 0;   p[9] = 0;   p[10] = 1;  p[11] = 0;
    p[12] = ox;  p[13] = oy;  p[14] = 0;  p[15] = 1;
    return m;
}

// -----------------------------------------------------------------------------
// Extern enums (matches your Rust set)
// -----------------------------------------------------------------------------
enum class TfxExtern : uint8_t {
    None = 0,
    Frame = 1,
    View = 2,
    Deferred = 3,
    DeferredLight = 4,
    DeferredUberLight = 5,
    DeferredShadow = 6,
    Atmosphere = 7,
    RigidModel = 8,
    EditorMesh = 9,
    EditorMeshMaterial = 10,
    EditorDecal = 11,
    EditorTerrain = 12,
    EditorTerrainPatch = 13,
    EditorTerrainDebug = 14,
    SimpleGeometry = 15,
    UiFont = 16,
    CuiView = 17,
    CuiObject = 18,
    CuiBitmap = 19,
    CuiVideo = 20,
    CuiStandard = 21,
    CuiHud = 22,
    CuiScreenspaceBoxes = 23,
    TextureVisualizer = 24,
    Generic = 25,
    Particle = 26,
    ParticleDebug = 27,
    GearDyeVisualizationMode = 28,
    ScreenArea = 29,
    Mlaa = 30,
    Msaa = 31,
    Hdao = 32,
    DownsampleTextureGeneric = 33,
    DownsampleDepth = 34,
    Ssao = 35,
    VolumetricObscurance = 36,
    Postprocess = 37,
    TextureSet = 38,
    Transparent = 39,
    Vignette = 40,
    GlobalLighting = 41,
    ShadowMask = 42,
    ObjectEffect = 43,
    Decal = 44,
    DecalSetTransform = 45,
    DynamicDecal = 46,
    DecoratorWind = 47,
    TextureCameraLighting = 48,
    VolumeFog = 49,
    Fxaa = 50,
    Smaa = 51,
    Letterbox = 52,
    DepthOfField = 53,
    PostprocessInitialDownsample = 54,
    CopyDepth = 55,
    DisplacementMotionBlur = 56,
    DebugShader = 57,
    MinmaxDepth = 58,
    SdsmBiasAndScale = 59,
    SdsmBiasAndScaleTextures = 60,
    ComputeShadowMapData = 61,
    ComputeLocalLightShadowMapData = 62,
    BilateralUpsample = 63,
    HealthOverlay = 64,
    LightProbeDominantLight = 65,
    LightProbeLightInstance = 66,
    Water = 67,
    LensFlare = 68,
    ScreenShader = 69,
    Scaler = 70,
    GammaControl = 71,
    SpeedtreePlacements = 72,
    Reticle = 73,
    Distortion = 74,
    WaterDebug = 75,
    ScreenAreaInput = 76,
    WaterDepthPrepass = 77,
    OverheadVisibilityMap = 78,
    ParticleCompute = 79,
    CubemapFiltering = 80,
    ParticleFastpath = 81,
    VolumetricsPass = 82,
    TemporalReprojection = 83,
    FxaaCompute = 84,
    VbCopyCompute = 85,
    UberDepth = 86,
    GearDye = 87,
    Cubemaps = 88,
    ShadowBlendWithPrevious = 89,
    DebugShadingOutput = 90,
    Ssao3d = 91,
    WaterDisplacement = 92,
    PatternBlending = 93,
    UiHdrTransform = 94,
    PlayerCenteredCascadedGrid = 95,
    SoftDeform = 96,
};

// -----------------------------------------------------------------------------
// Extern PODs with explicit offsets used by your bytecode
// -----------------------------------------------------------------------------
#pragma pack(push, 1)

struct FrameExtern {
    // 0x00
    float game_time = 0.0f;
    // 0x04
    float render_time = 0.0f;

    // 0x08..0x0B (gap)
    uint8_t _pad08[0x0C - 0x08]{};

    // 0x0C
    float  unk0c = 0.0f;
    // 0x10
    float  unk10 = 0.50f;
    // 0x14
    float  delta_game_time = 0.0f;
    // 0x18
    float  exposure_time = 0.0f;
    // 0x1C
    float  exposure_scale = 1.0f;
};

struct ViewExtern {
    float resolution_width = 0, resolution_height = 0;

    Mat4 world_to_camera;
    Mat4 camera_to_projective;

    Mat4 camera_to_world;
    Mat4 world_to_projective;
    Mat4 projective_to_world;
    Mat4 projective_to_camera;

    Mat4 target_pixel_to_camera;
    Mat4 target_pixel_to_world;
    Mat4 tptow_no_proj_w;

    Vec4 position;
    Vec4 unk30;
};

struct GlobalLightingExtern {
    uint8_t _pad00[0x30]{}; // 0x00..0x2F

    // 0x30 (Vec4)
    Vec4   unk30 = Vec4(1.0f, -1.0f, 1.0f, 0.0f);

    uint8_t _pad40[0x50 - 0x40]{}; // 0x40..0x4F

    // 0x50 (Vec4)
    Vec4   unk50 = Vec4(1.0f, -1.0f, 1.0f, 0.0f);

    uint8_t _pad60[0x70 - 0x60]{}; // 0x60..0x6F

    // 0x70/0x80 (Vec4)
    Vec4   unk70 = Vec4::zero();
    Vec4   unk80 = Vec4::zero();

    // 0x90..0xA0 (floats)
    float  unk90 = 0.0f;
    float  unk94 = -0.5f;
    float  unk98 = 0.0f;
    float  unk9c = 0.0f;
    float  unka0 = 0.0f;
};

struct DeferredExtern {
    Vec4  depth_constants = Vec4(0.0f, 1.0f / 0.01f, 0.0f, 0.0f);   // 0x00
    Vec4  unk10 = Vec4::zero();                         // 0x10
    Vec4  unk20 = Vec4::zero();                         // 0x20
    float unk30 = 0.0f;                                 // 0x30
    uint32_t _pad34 = 0;                                    // 0x34

    // SRVs (default nullptrs)
    ID3D11ShaderResourceView* deferred_depth = nullptr;     // 0x38
    ID3D11ShaderResourceView* deferred_rt0 = nullptr;     // 0x48
    ID3D11ShaderResourceView* deferred_rt1 = nullptr;     // 0x50
    ID3D11ShaderResourceView* deferred_rt2 = nullptr;     // 0x58
    ID3D11ShaderResourceView* light_diffuse = nullptr;     // 0x60
    ID3D11ShaderResourceView* light_specular = nullptr;     // 0x68
    ID3D11ShaderResourceView* light_ibl_specular = nullptr;     // 0x70
    ID3D11ShaderResourceView* unk78 = nullptr;     // 0x78
    ID3D11ShaderResourceView* unk80 = nullptr;     // 0x80
    ID3D11ShaderResourceView* unk88 = nullptr;     // 0x88
    ID3D11ShaderResourceView* unk90 = nullptr;     // 0x90
    ID3D11ShaderResourceView* sky_hemisphere_mips = nullptr;     // 0x98
};

struct ShadowMaskExtern {
    ID3D11ShaderResourceView* unk00 = nullptr; // 0x00
    ID3D11ShaderResourceView* unk08 = nullptr; // 0x08
    ID3D11ShaderResourceView* unk10 = nullptr; // 0x10
    Vec4  unk20 = Vec4::one();                // 0x20
    float unk30 = 0.0f;                       // 0x30
    float unk34 = 1.0f;                       // 0x34
    float _pad38[2] = {};
};

struct FxaaExtern {
    ID3D11ShaderResourceView* source_texture = nullptr; // 0x00
    float fxaa_param0 = 0.75f;                         // 0x50
    float fxaa_param1 = 1.0f / 6.0f;                     // 0x54
    float fxaa_param2 = 1.0f / 12.0f;                    // 0x58
    float _pad5c = 0.0f;
    float noise_time = 0.0f;                           // 0x80
    float _pad84[3] = {};
    Vec4  noise_intensity_scale = Vec4(0.25f, -0.225f, 0.40f, 0.96f); // 0x90
};

#pragma pack(pop)

// -----------------------------------------------------------------------------
// ExternStorage: owns bytes per extern and provides typed readers
// -----------------------------------------------------------------------------
struct ExternStorage {
    struct Blob { std::vector<uint8_t> bytes; };
    std::unordered_map<TfxExtern, Blob> blobs;

    template <class T>
    void set(TfxExtern id, const T& pod) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be POD/trivially copyable");
        Blob b; b.bytes.resize(sizeof(T));
        if constexpr (sizeof(T) > 0) {
            std::memcpy(b.bytes.data(), &pod, sizeof(T));
        }
        blobs[id] = std::move(b);
    }

    void set_raw(TfxExtern id, const void* p, size_t n) {
        Blob b; b.bytes.resize(n);
        if (n) std::memcpy(b.bytes.data(), p, n);
        blobs[id] = std::move(b);
    }

    // -------------------- readers --------------------
    float getFloat(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end()) return 0.0f;
        const auto& v = it->second.bytes;
        if (byteOffset + sizeof(float) > v.size()) return 0.0f;
        float out; std::memcpy(&out, v.data() + byteOffset, sizeof(out));
        return out;
    }

    uint32_t getU32(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end()) return 0u;
        const auto& v = it->second.bytes;
        if (byteOffset + sizeof(uint32_t) > v.size()) return 0u;
        uint32_t out; std::memcpy(&out, v.data() + byteOffset, sizeof(out));
        return out;
    }

    Vec4 getVec4(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end()) return Vec4::zero();
        const auto& v = it->second.bytes;
        if (byteOffset + sizeof(Vec4) > v.size()) return Vec4::zero();
        Vec4 out; std::memcpy(&out, v.data() + byteOffset, sizeof(out));
        return out;
    }

    Mat4 getMat4(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end()) return Mat4::identity();
        const auto& v = it->second.bytes;
        if (byteOffset + sizeof(Mat4) > v.size()) return Mat4::identity();
        Mat4 out; std::memcpy(&out, v.data() + byteOffset, sizeof(out));
        return out;
    }

    ID3D11ShaderResourceView* getSRV(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end() || byteOffset + sizeof(void*) > it->second.bytes.size()) return nullptr;
        ID3D11ShaderResourceView* srv{};
        std::memcpy(&srv, it->second.bytes.data() + byteOffset, sizeof(srv));
        return srv;
    }

    // -------------------- convenience seeders --------------------
    static ExternStorage FilledDefaults() {
        ExternStorage ex;
        ex.set(TfxExtern::Frame, FrameExtern{});
        ex.set(TfxExtern::View, ViewExtern{});
        ex.set(TfxExtern::Deferred, DeferredExtern{});
        ex.set(TfxExtern::ShadowMask, ShadowMaskExtern{});
        ex.set(TfxExtern::GlobalLighting, GlobalLightingExtern{});
        ex.set(TfxExtern::Fxaa, FxaaExtern{});
        return ex;
    }

    void SetFrameTimes(float game_time, float delta_game_time, float exposure_scale, float render_time = 0.0f) {
        FrameExtern f{};
        if (auto it = blobs.find(TfxExtern::Frame);
            it != blobs.end() && it->second.bytes.size() >= sizeof(FrameExtern)) {
            std::memcpy(&f, it->second.bytes.data(), sizeof(FrameExtern));
        }
        f.game_time = game_time;
        f.render_time = render_time;
        f.delta_game_time = delta_game_time;
        f.exposure_time = game_time;  // if you track differently, write that here
        f.exposure_scale = exposure_scale;
        set(TfxExtern::Frame, f);
    }

    void SetViewProjectiveToCamera(View& v, D3D11_VIEWPORT &vp) {
        v.resolution_width = vp.Width;
        v.resolution_height = vp.Height;

        v.camera_to_world = MInverse(v.world_to_camera);
        v.world_to_projective = MMul(v.camera_to_projective, v.world_to_camera);
        v.projective_to_world = MInverse(v.world_to_projective);
        v.projective_to_camera = MInverse(v.camera_to_projective);

        const XMFLOAT4X4 tptop = TargetPixelToProjective(vp.Width, vp.Height);
        v.target_pixel_to_camera = MMul(v.projective_to_camera, tptop);
        v.target_pixel_to_world = MMul(v.camera_to_world, v.target_pixel_to_camera);

        v.position = WAxis(v.camera_to_world);
        const XMFLOAT4 wproj = WAxis(v.world_to_projective);
        const XMFLOAT4 z = VecZ();
        v.unk30 = { z.x - wproj.x, z.y - wproj.y, z.z - wproj.z, z.w - wproj.w };

        const XMFLOAT4X4 ctow_noT = RemoveTranslation(v.camera_to_world);
        const XMFLOAT4X4 ptow_noT = MMul(ctow_noT, v.projective_to_camera);
        v.tptow_no_proj_w = MMul(ptow_noT, tptop);
    
        set(TfxExtern::View, v);
    }

    void SetGlobalLighting(const Vec4* specular_dir, const Vec4* diffuse_dir,
        const Vec4* v70, const Vec4* v80,
        const float* f90, const float* fA0) {
        GlobalLightingExtern g{};
        if (auto it = blobs.find(TfxExtern::GlobalLighting);
            it != blobs.end() && it->second.bytes.size() >= sizeof(GlobalLightingExtern)) {
            std::memcpy(&g, it->second.bytes.data(), sizeof(GlobalLightingExtern));
        }
        if (specular_dir) g.unk30 = *specular_dir;
        if (diffuse_dir)  g.unk50 = *diffuse_dir;
        if (v70)          g.unk70 = *v70;
        if (v80)          g.unk80 = *v80;
        if (f90)          g.unk90 = *f90;
        if (fA0)          g.unka0 = *fA0;
        set(TfxExtern::GlobalLighting, g);
    }

    void SetShadowMaskParams(const Vec4* v20, const float* f34) {
        ShadowMaskExtern sm{};
        if (auto it = blobs.find(TfxExtern::ShadowMask);
            it != blobs.end() && it->second.bytes.size() >= sizeof(ShadowMaskExtern)) {
            std::memcpy(&sm, it->second.bytes.data(), sizeof(ShadowMaskExtern));
        }
        if (v20) sm.unk20 = *v20;
        if (f34) sm.unk34 = *f34;
        set(TfxExtern::ShadowMask, sm);
    }

    void SetFxaa(float noise_time, const Vec4* intensity = nullptr) {
        FxaaExtern fx{};
        if (auto it = blobs.find(TfxExtern::Fxaa);
            it != blobs.end() && it->second.bytes.size() >= sizeof(FxaaExtern)) {
            std::memcpy(&fx, it->second.bytes.data(), sizeof(FxaaExtern));
        }
        fx.noise_time = noise_time;
        if (intensity) fx.noise_intensity_scale = *intensity;
        set(TfxExtern::Fxaa, fx);
    }
};

// extern_cb_manager.h
#pragma once
#include <unordered_map>
#include <vector>
#include <cstdint>
#include <algorithm>
#include <wrl/client.h>
#include <d3d11.h>

using Microsoft::WRL::ComPtr;

struct ExternCBManager
{
    struct Cb
    {
        ComPtr<ID3D11Buffer> gpu;     // D3D11 constant buffer
        std::vector<uint8_t> cpu;     // CPU mirror (what you memcpy into)
        bool dirty = false;
    };

    // One CB per extern "scope"
    std::unordered_map<TfxExtern, Cb> cbs;

    static inline uint32_t Align16(uint32_t v) { return (v + 15u) & ~15u; }

    // Create/resize a CB for 'id' from the bytes currently in storage (zeros if missing).
    // Safe to call repeatedly; it will reuse buffers of the same size.
    void EnsureFromStorage(ID3D11Device* dev, const ExternStorage& st, TfxExtern id)
    {
        const auto it = st.blobs.find(id);
        const size_t srcSize = (it == st.blobs.end()) ? 0u : it->second.bytes.size();
        const uint32_t byteWidth = std::max<uint32_t>(16u, Align16((uint32_t)srcSize)); // D3D11 CB must be multiple of 16 (and nonzero)
        Cb& cb = cbs[id];

        const bool needCreate = !cb.gpu || (cb.cpu.size() != byteWidth);
        if (needCreate)
        {
            // (Re)create GPU buffer
            D3D11_BUFFER_DESC bd{};
            bd.ByteWidth = byteWidth;
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.Usage = D3D11_USAGE_DYNAMIC;
            bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            // Initialize CPU mirror and (optionally) GPU with initial data
            cb.cpu.assign(byteWidth, 0u);
            if (srcSize)
                std::memcpy(cb.cpu.data(), it->second.bytes.data(), std::min<size_t>(srcSize, byteWidth));

            D3D11_SUBRESOURCE_DATA init{};
            init.pSysMem = cb.cpu.data();
            HRESULT hr = dev->CreateBuffer(&bd, &init, cb.gpu.ReleaseAndGetAddressOf());
            (void)hr; // handle/log if you prefer
            cb.dirty = false;
        }
        else
        {
            // Just refresh CPU mirror from storage if we already had a buffer
            if (srcSize)
            {
                std::memcpy(cb.cpu.data(), it->second.bytes.data(), std::min<size_t>(srcSize, cb.cpu.size()));
                if (cb.gpu) cb.dirty = true;
            }
        }
    }

    // Build/refresh **all** CBs that exist in storage
    void EnsureAllFromStorage(ID3D11Device* dev, const ExternStorage& st)
    {
        for (const auto& kv : st.blobs)
            EnsureFromStorage(dev, st, kv.first);
    }

    // memcpy into a scope's CPU bytes, clamped to the CB size.
    // Mark dirty so the next Upload* will push it to the GPU.
    void MemcpyScope(TfxExtern id, size_t dstOffset, const void* src, size_t numBytes)
    {
        auto it = cbs.find(id);
        if (it == cbs.end() || !src || numBytes == 0) return;

        Cb& cb = it->second;
        if (dstOffset >= cb.cpu.size()) return;

        const size_t n = std::min(numBytes, cb.cpu.size() - dstOffset);
        std::memcpy(cb.cpu.data() + dstOffset, src, n);
        cb.dirty = true;
    }

    // Replace an entire scope from raw data (also flags dirty)
    void SetScopeBytes(TfxExtern id, const void* src, size_t numBytes)
    {
        auto it = cbs.find(id);
        if (it == cbs.end()) return;

        Cb& cb = it->second;
        const size_t n = std::min(numBytes, cb.cpu.size());
        if (n && src) std::memcpy(cb.cpu.data(), src, n);
        if (n < cb.cpu.size()) std::memset(cb.cpu.data() + n, 0, cb.cpu.size() - n);
        cb.dirty = true;
    }

    // Upload one scope if dirty (Map WRITE_DISCARD then memcpy full range)
    void Upload(ID3D11DeviceContext* ctx, TfxExtern id)
    {
        auto it = cbs.find(id);
        if (it == cbs.end()) return;
        Cb& cb = it->second;
        if (!cb.gpu || !cb.dirty) return;

        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(ctx->Map(cb.gpu.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms)))
        {
            std::memcpy(ms.pData, cb.cpu.data(), cb.cpu.size());
            ctx->Unmap(cb.gpu.Get(), 0);
            cb.dirty = false;
        }
    }

    // Upload every dirty scope
    void UploadAll(ID3D11DeviceContext* ctx)
    {
        for (auto& kv : cbs) Upload(ctx, kv.first);
    }

    // Get raw buffer for binding
    ID3D11Buffer* GetBuffer(TfxExtern id) const
    {
        auto it = cbs.find(id);
        return (it == cbs.end()) ? nullptr : it->second.gpu.Get();
    }

    // Convenience: bind to a stage/slot
    void BindPS(ID3D11DeviceContext* ctx, UINT slot, TfxExtern id) const
    {
        ID3D11Buffer* b = GetBuffer(id);
        ctx->PSSetConstantBuffers(slot, 1, &b);
    }
    void BindVS(ID3D11DeviceContext* ctx, UINT slot, TfxExtern id) const
    {
        ID3D11Buffer* b = GetBuffer(id);
        ctx->VSSetConstantBuffers(slot, 1, &b);
    }
};
