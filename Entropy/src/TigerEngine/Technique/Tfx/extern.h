#pragma once
#include <unordered_map>
#include <vector>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include "tfx_runtime.h"
#include <d3d11.h>

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
    float resolution_width = 0.0f;  // 0x00
    float resolution_height = 0.0f;  // 0x04
    float _pad08[2] = {};    // 0x08
    Mat4  world_to_camera = Mat4::identity();      // 0x40
    Mat4  camera_to_projective = Mat4::identity();    // 0x80
    Mat4  camera_to_world = Mat4::identity();      // 0xC0
    Mat4  projective_to_camera = Mat4::identity();    // 0x100
    Mat4  world_to_projective = Mat4::identity();    // 0x140
    Mat4  projective_to_world = Mat4::identity();    // 0x180
    Mat4  target_pixel_to_world = Mat4::identity();   // 0x1C0
    Mat4  target_pixel_to_camera = Mat4::identity();  // 0x200
    Mat4  tptow_no_proj_w = Mat4::identity();   // 0x280
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

    void SetViewProjectiveToCamera(const Mat4& ptoc, float width, float height) {
        ViewExtern v{};
        if (auto it = blobs.find(TfxExtern::View);
            it != blobs.end() && it->second.bytes.size() >= sizeof(ViewExtern)) {
            std::memcpy(&v, it->second.bytes.data(), sizeof(ViewExtern));
        }
        v.resolution_width = width;
        v.resolution_height = height;
        v.projective_to_camera = ptoc;
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
