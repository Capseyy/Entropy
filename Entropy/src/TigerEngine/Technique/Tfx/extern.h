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
#include <wrl/client.h>
#include <limits>
#undef min
#undef max
#include "Renderer/Graphics/Scope/view.h"

inline float qNaN() { return std::numeric_limits<float>::quiet_NaN(); }



#pragma pack(push, 1)
struct TransparentExtern
{
    // --- SRVs (TextureView) ---
    // 0x00..0x67 (8-byte pointers on x64)
    ID3D11ShaderResourceView* atmos_ss_far_lookup = nullptr; // 0x00
    ID3D11ShaderResourceView* atmos_ss_far_lookup_downsampled = nullptr; // 0x08
    ID3D11ShaderResourceView* atmos_ss_near_lookup = nullptr; // 0x10
    ID3D11ShaderResourceView* atmos_ss_near_lookup_downsampled = nullptr; // 0x18
    ID3D11ShaderResourceView* atmosphere_depth_angle_density_lookup = nullptr; // 0x20
    ID3D11ShaderResourceView* unk28 = nullptr; // 0x28 (Texture3D)
    ID3D11ShaderResourceView* unk30 = nullptr; // 0x30 (Texture3D)
    ID3D11ShaderResourceView* unk38 = nullptr; // 0x38 (Texture3D)
    ID3D11ShaderResourceView* unk40 = nullptr; // 0x40 (light_grid_shadow_final 3D)
    ID3D11ShaderResourceView* unk48 = nullptr; // 0x48 (volumetrics surface0 / result 2D)
    ID3D11ShaderResourceView* unk50 = nullptr; // 0x50 (volumetrics intensity 3D)
    ID3D11ShaderResourceView* unk58 = nullptr; // 0x58
    ID3D11ShaderResourceView* unk60 = nullptr; // 0x60 (shading_result_read)

    // Align up to 0x70 for the next Vec4 block (0x68..0x6F are padding)
    uint8_t _pad68[0x70 - 0x68]{};

    // --- Vec4s ---
    // 0x70..0xBF
    Vec4  unk70 = Vec4::one(); // left as ONE by default unless you have better values
    Vec4  unk80 = Vec4::one();
    Vec4  unk90 = Vec4::one();
    Vec4  unka0 = Vec4::one();
    Vec4  unkb0 = Vec4::one();
};


inline TransparentExtern MakeTransparentExternDefaults() {
    return TransparentExtern{}; // all SRVs nullptr; vec4s = ONE
};


struct FrameAuxCB
{
    // scalars (pad to 16B)
    Vec4 times;
    float exposure_time = 1.0f;         // 0x0C
    float exposure_illum_relative_glow = 16.0f;   // 0x00
    float exposure_scale_for_shading = 1.0f;   // 0x04
    float exposure_illum_relative = 1.0f;   // 0x08
    

    // vectors
    Vec4  random_seed_scales = Vec4(102.8505f, 102.04853f, 943.28906f, 187.40677f); // 0x10
    Vec4  overrides = Vec4(0.5f, 0.5f, 0.0f, 0.0f);                        // 0x20
    Vec4  unk4 = Vec4(1.0f, 1.0f, 0.0f, 1.0f);                        // 0x30
    Vec4  unk5 = Vec4(0.0f, std::numeric_limits<float>::quiet_NaN(), 512.0f, 0.0f); // 0x40
    Vec4  unk6 = Vec4(0.0f, 1.0f, 0.9667876f, 0.0f);                  // 0x50
    Vec4  unk7 = Vec4(0.0f, 0.5f, 180.0f, 0.0f);                      // 0x60
    Vec4  unk8 = Vec4::zero();                                        // 0x70
    Vec4  unk9 = Vec4::zero();                                        // 0x80
    Vec4  unka = Vec4(std::numeric_limits<float>::quiet_NaN(), 0.0f, 0.0f, 0.0f);  // 0x90
};
#pragma pack(pop)

using Microsoft::WRL::ComPtr;

// --------------------------------- math helpers ---------------------------------
inline XMMATRIX XM(const Mat4& m) { return XMLoadFloat4x4(reinterpret_cast<const XMFLOAT4X4*>(&m)); }
inline Mat4     M4(FXMMATRIX m) { Mat4 out; XMStoreFloat4x4(reinterpret_cast<XMFLOAT4X4*>(&out), m); return out; }

inline XMMATRIX   ToXM(const XMFLOAT4X4& m) { return XMLoadFloat4x4(&m); }
inline XMFLOAT4X4 FromXM(FXMMATRIX m) { XMFLOAT4X4 r; XMStoreFloat4x4(&r, m); return r; }

inline XMFLOAT4X4 MIdentity() { return FromXM(XMMatrixIdentity()); }
inline XMFLOAT4X4 MMul(const XMFLOAT4X4& a, const XMFLOAT4X4& b) { return FromXM(XMMatrixMultiply(ToXM(a), ToXM(b))); }
inline XMFLOAT4X4 MInverse(const XMFLOAT4X4& m) { XMVECTOR det; return FromXM(XMMatrixInverse(&det, ToXM(m))); }

// translation row-major (last row)
inline XMFLOAT4 WAxis(const XMFLOAT4X4& m) { return { m._41, m._42, m._43, m._44 }; }
inline XMFLOAT4 VecZ() { return { 0,0,1,0 }; }

inline XMFLOAT4X4 RemoveTranslation(const XMFLOAT4X4& m) {
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

// Mat4 helpers
inline Mat4 inverse(const Mat4& m) { XMVECTOR det{}; return M4(XMMatrixInverse(&det, XM(m))); }
inline Mat4 mul(const Mat4& a, const Mat4& b) { return M4(XMMatrixMultiply(XM(a), XM(b))); }

inline Vec4 w_axis_rowmajor(const Mat4& m) {
    const float* p = reinterpret_cast<const float*>(&m);
    return Vec4(p[12], p[13], p[14], p[15]);
}
inline Vec4 vec4_Z() { return Vec4(0.f, 0.f, 1.f, 0.f); }

inline Mat4 remove_translation_rowmajor(Mat4 m) {
    float* p = reinterpret_cast<float*>(&m);
    p[12] = p[13] = p[14] = 0.f;
    p[15] = 1.f;
    return m;
}

inline Mat4 target_pixel_to_projective(float W, float H) {
    const float sx = 2.0f / W;
    const float sy = -2.0f / H;
    const float ox = sx * 0.5f - 1.0f;
    const float oy = 1.0f - sy * 0.5f;

    Mat4 m = Mat4::identity();
    float* p = reinterpret_cast<float*>(&m);
    p[0] = sx;  p[1] = 0;   p[2] = 0;  p[3] = 0;
    p[4] = 0;   p[5] = sy;  p[6] = 0;  p[7] = 0;
    p[8] = 0;   p[9] = 0;   p[10] = 1; p[11] = 0;
    p[12] = ox; p[13] = oy; p[14] = 0; p[15] = 1;
    return m;
}

// --------------------------------- extern kinds ---------------------------------
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
    DrawingShader = 24,
    TextureVisualizer = 25,
    Generic = 26,
    Particle = 27,
    ParticleDebug = 28,
    GearDyeVisualizationMode = 29,
    ScreenArea = 30,
    Mlaa = 31,
    Msaa = 32,
    Hdao = 33,
    DownsampleTextureGeneric = 34,
    DownsampleDepth = 35,
    Ssao = 36,
    VolumetricObscurance = 37,
    Postprocess = 38,
    TextureSet = 39,
    Transparent = 40,
    Vignette = 41,
    GlobalLighting = 42,
    ShadowMask = 43,
    ObjectEffect = 44,
    Decal = 45,
    DecalSetTransform = 46,
    DynamicDecal = 47,
    DecoratorWind = 48,
    TextureCameraLighting = 49,
    VolumeFog = 50,
    Fxaa = 51,
    Smaa = 52,
    Letterbox = 53,
    DepthOfField = 54,
    PostprocessInitialDownsample = 55,
    CopyDepth = 56,
    DisplacementMotionBlur = 57,
    DebugShader = 58,
    MinmaxDepth = 59,
    SdsmBiasAndScale = 60,
    SdsmBiasAndScaleTextures = 61,
    ComputeShadowMapData = 62,
    ComputeLocalLightShadowMapData = 63,
    BilateralUpsample = 64,
    HealthOverlay = 65,
    LightProbeDominantLight = 66,
    LightProbeLightInstance = 67,
    Water = 68,
    LensFlare = 69,
    ScreenShader = 70,
    Scaler = 71,
    GammaControl = 72,
    SpeedtreePlacements = 73,
    Reticle = 74,
    Distortion = 75,
    WaterDebug = 76,
    ScreenAreaInput = 77,
    WaterDepthPrepass = 78,
    OverheadVisibilityMap = 79,
    ParticleCompute = 80,
    CubemapFiltering = 81,
    ParticleFastpath = 82,
    VolumetricsPass = 83,
    TemporalReprojection = 84,
    FxaaCompute = 85,
    VbCopyCompute = 86,
    UberDepth = 87,
    GearDye = 88,
    Cubemaps = 89,
    ShadowBlendWithPrevious = 90,
    DebugShadingOutput = 91,
    Ssao3d = 92,
    WaterDisplacement = 93,
    PatternBlending = 94,
    UiHdrTransform = 95,
    PlayerCenteredCascadedGrid = 96,
    SoftDeform = 97,
};



// --------------------------------- packed PODs ---------------------------------
#pragma pack(push, 1)
struct FrameExtern
{
    // --- scalars ---
    float game_time = 0.0f;   // 0x00
    float render_time = 0.0f;   // 0x04
    uint8_t _pad08[0x0C - 0x08]{};           // 0x08..0x0B
    float unk0c = 0.0f;   // 0x0C  (unimplemented)
    float unk10 = 0.50f;  // 0x10  (default 0.50)
    float delta_game_time = 0.0f;   // 0x14  (unimplemented)
    float exposure_time = 0.0f;   // 0x18  (unimplemented)
    float exposure_scale = 1.0f;   // 0x1C
    float unk20 = 0.0f;   // 0x20  (unimplemented)
    float unk24 = 0.0f;   // 0x24  (unimplemented)
    float exposure_illum_relative = 0.0f;   // 0x28  (unimplemented)
    float unk2c = 0.0f;   // 0x2C  (unimplemented)

    uint8_t _pad30[0x40 - 0x30]{};           // 0x30..0x3F
    float   unk40 = 0.0f;   // 0x40  (unimplemented)

    uint8_t _pad44[0x70 - 0x44]{};           // 0x44..0x6F
    float   unk70 = 0.0f;   // 0x70  (unimplemented)

    uint8_t _pad74[0x78 - 0x74]{};           // 0x74..0x77

    // --- TextureView (SRV*) ---
    ID3D11ShaderResourceView* unk78 = nullptr; // 0x78  (unimplemented)
    ID3D11ShaderResourceView* unk80 = nullptr; // 0x80  (unimplemented)
    ID3D11ShaderResourceView* unk88 = nullptr; // 0x88  (unimplemented)
    ID3D11ShaderResourceView* unk90 = nullptr; // 0x90  (unimplemented)
    ID3D11ShaderResourceView* unk98 = nullptr; // 0x98  (unimplemented)
    ID3D11ShaderResourceView* unka0 = nullptr; // 0xA0  (unimplemented)
    ID3D11ShaderResourceView* specular_lobe_lookup = nullptr; // 0xA8
    ID3D11ShaderResourceView* specular_lobe_3d_lookup = nullptr; // 0xB0
    ID3D11ShaderResourceView* specular_tint_lookup = nullptr; // 0xB8
    ID3D11ShaderResourceView* iridescence_lookup = nullptr; // 0xC0

    uint8_t _padC8[0xD0 - 0xC8]{};           // 0xC8..0xCF

    // --- vectors ---
    Vec4    unkd0 = Vec4::zero();                 // 0xD0  (unimplemented)
    uint8_t _padE0[0x150 - 0xE0]{};          // 0xE0..0x14F
    Vec4    unk150 = Vec4::zero();                 // 0x150 (unimplemented)
    Vec4    unk160 = Vec4::zero();                 // 0x160 (unimplemented)
    Vec4    unk170 = Vec4::zero();                 // 0x170 (unimplemented)
    Vec4    unk180 = Vec4::zero();                 // 0x180 (unimplemented)

    // --- more scalars ---
    float   unk190 = 0.0f;                         // 0x190 (unimplemented)
    float   unk194 = 0.0f;                         // 0x194 (unimplemented)

    uint8_t _pad198[0x1A0 - 0x198]{};        // 0x198..0x19F

    // --- vectors with known defaults ---
    Vec4    unk1a0 = Vec4::one();                 // 0x1A0 default ZERO
    Vec4    unk1b0 = Vec4::one();                 // 0x1B0
    Vec4    unk1c0 = Vec4(1.0f, 1.0f, 0.0f, 1.0f); // 0x1C0 default (1,1,0,1)

    uint8_t _pad1D0[0x1E0 - 0x1D0]{};        // 0x1D0..0x1DF
    ID3D11ShaderResourceView* unk1e0 = nullptr;             // 0x1E0 (unimplemented)
    ID3D11ShaderResourceView* unk1e8 = nullptr;             // 0x1E8 (unimplemented)
    ID3D11ShaderResourceView* unk1f0 = nullptr;             // 0x1F0 (unimplemented)
};
#pragma pack(pop)

// Your ScopeFrame defaults, applied to the current FrameExtern layout.
inline FrameExtern MakeScopeFrameDefaults()
{
    FrameExtern f{}; // starts from zero/nullptr, then override below.

    // Times / exposure
    f.game_time = 0.0f;
    f.render_time = 0.0f;
    f.delta_game_time = 0.0f;
    f.exposure_time = 1.0f / 60.0f;          // ScopeFrame::exposure_time
    f.exposure_scale = 1.0f;                  // ScopeFrame::exposure_scale
    f.exposure_illum_relative = 1.4616859f;        // ScopeFrame::exposure_illum_relative

    // “Glow” & “for_shading” best-guess slots
    f.unk0c = 23.386974f;                          // ScopeFrame::exposure_illum_relative_glow
    f.unk10 = 0.50f;                               // keep default
    f.unk20 = 0.5674782f;                          // ScopeFrame::exposure_scale_for_shading

    // Vec4 block
    f.unkd0 = Vec4(102.8505f, 102.04853f, 943.28906f, 187.40677f); // random_seed_scales
    f.unk150 = Vec4(0.0f, qNaN(), 512.0f, 0.0f);                     // unk5
    f.unk160 = Vec4(0.0f, 1.0f, 0.9667876f, 0.0f);                   // unk6
    f.unk170 = Vec4(0.0f, 0.5f, 180.0f, 0.0f);                       // unk7
    f.unk180 = Vec4::zero();                                         // unk8

    // Scalars around 0x190—left at defaults unless you want to bind something specific
    f.unk190 = 1.0f;
    f.unk194 = 1.0f;

    f.unk1a0 = Vec4(0.5f, 0.5f, 0.0f, 0.0f);                         // overrides
    f.unk1b0 = Vec4::zero();                                         // unk9
    f.unk1c0 = Vec4(1.0f, 1.0f, 0.0f, 1.0f);                         // unk4 (matches comment)

    // If you also need ScopeFrame::unka = (NaN,0,0,0), repurpose one slot, e.g.:
    // f.unk180 = Vec4(qNaN(), 0.0f, 0.0f, 0.0f);  // (alternative placement)

    return f;
}



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


#pragma pack(push, 1)
struct GlobalLightingExtern {
    // 0x00..0x07
    uint8_t _pad00[0x08]{};

    // 0x08
    ID3D11ShaderResourceView* unk08 = nullptr;

    // 0x10..0x1F
    Vec4  unk10 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default

    // 0x20..0x2F
    uint8_t _pad20[0x30 - 0x20]{};

    // 0x30..0x3F  (specular light dir)
    Vec4  unk30 = Vec4(1.0f, -1.0f, 1.0f, 0.0f);

    // 0x40..0x4F
    uint8_t _pad40[0x50 - 0x40]{};

    // 0x50..0x5F  (diffuse light dir)
    Vec4  unk50 = Vec4(1.0f, -1.0f, 1.0f, 0.0f);

    // 0x60..0x6F
    uint8_t _pad60[0x70 - 0x60]{};

    // 0x70..0x7F
    Vec4  unk70 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default

    // 0x80..0x8F
    Vec4  unk80 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default

    // 0x90..0xA3 (scalars)
    float unk90 = 1.0f;                                // unimplemented(true) -> f32 default 1.0
    float unk94 = -0.5f;                               // explicit default(-0.5)
    float unk98 = 1.0f;                                // unimplemented(true) -> f32 default 1.0
    float unk9c = 1.0f;                                // unimplemented(true) -> f32 default 1.0
    float unka0 = 1.0f;                                // unimplemented(true) -> f32 default 1.0

    // 0xA4..0xAF
    uint8_t _padA4[0xB0 - 0xA4]{};

    // 0xB0..0xDF
    Vec4  unkb0 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default
    Vec4  unkc0 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default
    Vec4  unkd0 = Vec4::one();                         // unimplemented(true) -> Vec4::ONE default
};
#pragma pack(pop)


struct DeferredExtern {
    Vec4  depth_constants = Vec4(0.0f, 1.0f / 0.01f, 0.0f, 0.0f);   // 0x00
    Vec4  unk10 = Vec4::zero();                                      // 0x10
    Vec4  unk20 = Vec4::zero();                                      // 0x20
    float unk30 = 0.0f;                                              // 0x30
    uint32_t _pad34 = 0;                                             // 0x34
    // SRVs
    ID3D11ShaderResourceView* deferred_depth = nullptr;              // 0x38
    ID3D11ShaderResourceView* deferred_rt0 = nullptr;              // 0x48
    ID3D11ShaderResourceView* deferred_rt1 = nullptr;              // 0x50
    ID3D11ShaderResourceView* deferred_rt2 = nullptr;              // 0x58
    ID3D11ShaderResourceView* light_diffuse = nullptr;              // 0x60
    ID3D11ShaderResourceView* light_specular = nullptr;              // 0x68
    ID3D11ShaderResourceView* light_ibl_specular = nullptr;          // 0x70
    ID3D11ShaderResourceView* unk78 = nullptr;                       // 0x78
    ID3D11ShaderResourceView* unk80 = nullptr;                       // 0x80
    ID3D11ShaderResourceView* unk88 = nullptr;                       // 0x88
    ID3D11ShaderResourceView* unk90 = nullptr;                       // 0x90
    ID3D11ShaderResourceView* sky_hemisphere_mips = nullptr;         // 0x98
};

#pragma pack(push, 1)
struct AtmosphereExtern {
    // 0x00..0x1F
    ID3D11ShaderResourceView* unk00 = nullptr; // 0x00
    ID3D11ShaderResourceView* unk08 = nullptr; // 0x08
    ID3D11ShaderResourceView* unk10 = nullptr; // 0x10
    ID3D11ShaderResourceView* unk18 = nullptr; // 0x18

    uint8_t _pad20[0x40 - 0x20]{};             // 0x20..0x3F (gap noted in your comment)

    // 0x40..0x6F
    ID3D11ShaderResourceView* unk40 = nullptr; // 0x40
    uint8_t _pad48[0x58 - 0x48]{};
    ID3D11ShaderResourceView* unk58 = nullptr; // 0x58
    uint8_t _pad60[0x70 - 0x60]{};

    // 0x70..0x8F
    float time_of_day_normalized = 0.5f;       // 0x70 (default 0.5)
    float unk74 = 0.0f;                        // 0x74
    float unk78 = 0.0f;                        // 0x78
    uint8_t _pad7C[0x80 - 0x7C]{};
    ID3D11ShaderResourceView* unk80 = nullptr; // 0x80
    ID3D11ShaderResourceView* unk88 = nullptr; // 0x88

    // 0x90..0xAF
    Vec4 atmosphere_lookup_resolution = Vec4::zero(); // 0x90
    ID3D11ShaderResourceView* light_shaft_optical_depth = nullptr; // 0xA0
    uint8_t _padA8[0xC0 - 0xA8]{};

    // 0xC0..0xCF
    ID3D11ShaderResourceView* unkc0 = nullptr; // 0xC0

    // 0xD0..0xDF
    // default: (512, 512, 1/512, 1/512)
    Vec4 depth_angle_density_lookup_resolution = Vec4(512.0f, 512.0f, 1.0f / 512.0f, 1.0f / 512.0f); // 0xD0

    // 0xE0..0xFF
    ID3D11ShaderResourceView* atmos_ss_far_lookup = nullptr;             // 0xE0
    ID3D11ShaderResourceView* atmos_ss_far_lookup_downsampled = nullptr; // 0xE8
    ID3D11ShaderResourceView* atmos_ss_near_lookup = nullptr;            // 0xF0
    ID3D11ShaderResourceView* atmos_ss_near_lookup_downsampled = nullptr;// 0xF8

    // 0x100..0x13F
    ID3D11ShaderResourceView* unk100 = nullptr; // 0x100
    Vec4  unk110 = Vec4(0.0f, 0.0f, -1.5f, 0.0f); // 0x110 (Vec4::Z * -1.5)
    uint8_t _pad120[0x140 - 0x120]{};

    // 0x140..0x17F
    Vec4  fog_color = Vec4::zero(); // 0x140
    float unk150 = 0.0f;            // 0x150
    float unk154 = 0.0f;            // 0x154
    float fog_intensity = 0.0f;     // 0x160 (default 0.0)
    float unk164 = 0.0f;            // 0x164
    float unk168 = 0.0f;            // 0x168
    float unk16c = 0.0f;            // 0x16C
    float unk170 = 0.0001f;         // 0x170 (default 1e-4)
    uint8_t _pad174[0x180 - 0x174]{};
    Vec4  unk180 = Vec4::zero();    // 0x180

    // 0x190..0x1AF
    float unk190 = 0.0f;            // 0x190
    float unk194 = 0.0f;            // 0x194
    float unk198 = 0.0001f;         // 0x198 (default 1e-4)
    uint8_t _pad19C[0x1B4 - 0x19C]{};

    // 0x1B4..0x1CF
    float unk1b4_rotation = 0.0f;   // 0x1B4 (default 0.0)
    float unk1b8_intensity = 0.0f;  // 0x1B8
    float unk1bc = 0.5f;            // 0x1BC (default 0.5)
    float unk1c0 = 0.0f;            // 0x1C0
    float unk1c4 = 0.0f;            // 0x1C4
    uint8_t _pad1C8[0x1D0 - 0x1C8]{};

    // 0x1D0..0x20F
    Vec4  unk1d0 = Vec4::zero();    // 0x1D0 (default ZERO)
    float unk1e0 = 0.0f;            // 0x1E0
    float unk1e4 = 0.0f;            // 0x1E4
    float unk1e8 = 0.0f;            // 0x1E8 (default 0.0)
    float unk1ec = 0.0f;            // 0x1EC
    uint8_t _pad1F0[0x1F8 - 0x1F0]{};
    float unk1f8 = 0.0f;            // 0x1F8
    float unk1fc = 0.0f;            // 0x1FC
    uint8_t _pad200[0x208 - 0x200]{};
    float unk208 = 0.0f;            // 0x208
    uint8_t _pad20C[0x210 - 0x20C]{};
    Vec4  unk210 = Vec4::zero();    // 0x210
};
#pragma pack(pop)

struct ShadowMaskExtern {
    ID3D11ShaderResourceView* unk00 = nullptr; // 0x00
    ID3D11ShaderResourceView* unk08 = nullptr; // 0x08
    ID3D11ShaderResourceView* unk10 = nullptr; // 0x10
    ID3D11ShaderResourceView* unk18 = nullptr;
    Vec4  unk20 = Vec4(1.0f, 1.0f, 1.0f, 1.0f);             // 0x20
    float unk30 = 1.0f;                        // 0x30
    float unk34 = 1.0f;                        // 0x34
    float _pad38[2] = {};
};

inline ShadowMaskExtern MakeShadowMaskAllOnes(ID3D11ShaderResourceView* whiteSRV = nullptr)
{
    ShadowMaskExtern sm{};
    // If you have a 1×1 white texture SRV, pass it here; otherwise these stay null.
    sm.unk00 = whiteSRV;
    sm.unk08 = whiteSRV;
    sm.unk10 = whiteSRV;
    sm.unk18 = whiteSRV;

    // All numeric knobs to 1.0
    sm.unk20 = Vec4::one();
    sm.unk30 = 1.0f;
    sm.unk34 = 1.0f;
    return sm;
}

#pragma pack(push, 1)
struct SimpleGeometryExtern {
    Mat4 transform = Mat4::identity(); // 0x00
};
#pragma pack(pop)

struct FxaaExtern {
    ID3D11ShaderResourceView* source_texture = nullptr; // 0x00
    float fxaa_param0 = 0.75f;                          // 0x50
    float fxaa_param1 = 1.0f / 6.0f;                    // 0x54
    float fxaa_param2 = 1.0f / 12.0f;                   // 0x58
    float _pad5c = 0.0f;
    float noise_time = 0.0f;                            // 0x80
    float _pad84[3] = {};
    Vec4  noise_intensity_scale = Vec4(0.25f, -0.225f, 0.40f, 0.96f); // 0x90
};
#pragma pack(pop)

inline GlobalLightingExtern MakeGlobalLightingDefaults() {
    return GlobalLightingExtern{}; // in-class defaults already match the Rust defaults
}

#pragma pack(push, 1)
struct DeferredLightExtern {
    // 0x00..0x3F — unknown header in the CB we don't use here
    uint8_t _pad00[0x40]{};

    // 0x40
    Mat4  unk40;   // used (Alkahest: translate(view.position))

    // 0x80
    Mat4  unk80;// used (Alkahest: node local_to_world)

    // 0xC0..0xFF
    Vec4  unkc0 = Vec4::one();        // defaults to 1,1,1,1
    Vec4  unkd0 = Vec4::one();
    Vec4  unke0 = Vec4::one();
    Vec4  unkf0 = Vec4::one();

    // 0x100
    Vec4  unk100 = Vec4::one();       // Alkahest: light params (leave 1s by default)

    // 0x110..0x120
    float unk110 = 1.0f;              // unknown ? 1.0
    float unk114 = 7500.0f;           // known default
    float unk118 = 1.0f;              // unknown ? 1.0
    float unk11c = 1.0f;              // unknown ? 1.0
    float unk120 = 1.0f;              // unknown ? 1.0
};
#pragma pack(pop)


// ------------------------------ ONE CLASS FOR ALL SCOPES ------------------------------
struct ExternStorage
{
    struct Scope {
        std::vector<uint8_t> cpu;    // CPU mirror
        ComPtr<ID3D11Buffer> gpu;    // D3D11 constant buffer
        bool dirty = false;
    };

    std::unordered_map<TfxExtern, Scope> scopes;

    // -------- utilities --------
    static inline uint32_t Align16(uint32_t v) { return (v + 15u) & ~15u; }

    // -------- typed setters --------
    template <class T>
    void set(TfxExtern id, const T& pod) {
        static_assert(std::is_trivially_copyable<T>::value, "T must be POD/trivially copyable");
        Scope& s = scopes[id];
        s.cpu.resize(sizeof(T));
        if constexpr (sizeof(T) > 0) std::memcpy(s.cpu.data(), &pod, sizeof(T));
        s.dirty = true;
    }

    void set_raw(TfxExtern id, const void* p, size_t n) {
        Scope& s = scopes[id];
        s.cpu.resize(n);
        if (n && p) std::memcpy(s.cpu.data(), p, n);
        s.dirty = true;
    }

    inline DeferredLightExtern GetDeferredLight() {
        DeferredLightExtern d{};
        auto it = scopes.find(TfxExtern::DeferredLight);
        if (it != scopes.end() && it->second.cpu.size() >= sizeof(DeferredLightExtern))
            std::memcpy(&d, it->second.cpu.data(), sizeof(DeferredLightExtern));
        return d;
    }

    inline void SetDeferredLight(const Mat4& unk40,
        const Mat4& unk80,
        const Vec4& unk100)
    {
        DeferredLightExtern d = GetDeferredLight(); // preserve existing fields
        d.unk40 = unk40;
        d.unk80 = unk80;
        d.unk100 = unk100;
        set(TfxExtern::DeferredLight, d);
    }

    // -------- readers (from CPU bytes) --------
    float getFloat(TfxExtern id, size_t byteOffset) const {
        auto it = scopes.find(id); if (it == scopes.end()) return 0.0f;
        const auto& v = it->second.cpu; if (byteOffset + sizeof(float) > v.size()) return 0.0f;
        float out; std::memcpy(&out, v.data() + byteOffset, sizeof(out)); return out;
    }
    uint32_t getU32(TfxExtern id, size_t byteOffset) const {
        auto it = scopes.find(id); if (it == scopes.end()) return 0u;
        const auto& v = it->second.cpu; if (byteOffset + sizeof(uint32_t) > v.size()) return 0u;
        uint32_t out; std::memcpy(&out, v.data() + byteOffset, sizeof(out)); return out;
    }
    Vec4 getVec4(TfxExtern id, size_t byteOffset) const {
        auto it = scopes.find(id); if (it == scopes.end()) return Vec4::zero();
        const auto& v = it->second.cpu; if (byteOffset + sizeof(Vec4) > v.size()) return Vec4::zero();
        Vec4 out; std::memcpy(&out, v.data() + byteOffset, sizeof(out)); return out;
    }
    Mat4 getMat4(TfxExtern id, size_t byteOffset) const {
        auto it = scopes.find(id); if (it == scopes.end()) return Mat4::identity();
        const auto& v = it->second.cpu; if (byteOffset + sizeof(Mat4) > v.size()) return Mat4::identity();
        Mat4 out; std::memcpy(&out, v.data() + byteOffset, sizeof(out)); return out;
    }
    ID3D11ShaderResourceView* getSRV(TfxExtern id, size_t byteOffset) const {
        auto it = scopes.find(id);
        if (it == scopes.end() || byteOffset + sizeof(void*) > it->second.cpu.size()) return nullptr;
        ID3D11ShaderResourceView* srv{};
        std::memcpy(&srv, it->second.cpu.data() + byteOffset, sizeof(srv));
        return srv;
    }

    // -------- ensure / upload (GPU) --------
    void Ensure(ID3D11Device* dev, TfxExtern id) {
        Scope& s = scopes[id];
        const uint32_t srcSize = static_cast<uint32_t>(s.cpu.size());
        const uint32_t byteWide = std::max<uint32_t>(16u, Align16(srcSize));

        const bool needCreate = (!s.gpu) || (GetByteWidth(s.gpu.Get()) != byteWide);
        if (needCreate) {
            D3D11_BUFFER_DESC bd{}; bd.ByteWidth = byteWide;
            bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
            bd.Usage = D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            // Prepare initial CPU bytes (padded/truncated)
            std::vector<uint8_t> initCpu(byteWide, 0u);
            if (srcSize) std::memcpy(initCpu.data(), s.cpu.data(), std::min<size_t>(srcSize, byteWide));
            s.cpu = initCpu; // keep mirror the same width

            D3D11_SUBRESOURCE_DATA init{}; init.pSysMem = s.cpu.data();
            dev->CreateBuffer(&bd, &init, s.gpu.ReleaseAndGetAddressOf());
            s.dirty = false;
        }
        else {
            // CPU mirror must be at least the current GPU width
            const uint32_t have = static_cast<uint32_t>(s.cpu.size());
            if (have < byteWide) s.cpu.resize(byteWide, 0);
            s.dirty = true; // new data present; push on next Upload
        }
    }

    void EnsureAll(ID3D11Device* dev) {
        for (auto& kv : scopes) Ensure(dev, kv.first);
    }

    void Upload(ID3D11DeviceContext* ctx, TfxExtern id) {
        auto it = scopes.find(id); if (it == scopes.end()) return;
        Scope& s = it->second;
        if (!s.gpu || !s.dirty) return;

        D3D11_MAPPED_SUBRESOURCE ms{};
        if (SUCCEEDED(ctx->Map(s.gpu.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
            std::memcpy(ms.pData, s.cpu.data(), s.cpu.size());
            ctx->Unmap(s.gpu.Get(), 0);
            s.dirty = false;
        }
    }

    void UploadAll(ID3D11DeviceContext* ctx) {
        for (auto& kv : scopes) Upload(ctx, kv.first);
    }

    // Raw memcpy into a scope's CPU buffer (clamped)
    void MemcpyScope(TfxExtern id, size_t dstOffset, const void* src, size_t numBytes) {
        auto it = scopes.find(id); if (it == scopes.end() || !src || !numBytes) return;
        Scope& s = it->second; if (dstOffset >= s.cpu.size()) return;
        const size_t n = std::min(numBytes, s.cpu.size() - dstOffset);
        std::memcpy(s.cpu.data() + dstOffset, src, n);
        s.dirty = true;
    }

    void SetSimpleGeometryTransform(const Mat4& transform) {
        SimpleGeometryExtern sg{};
        sg.transform = transform;
        set(TfxExtern::SimpleGeometry, sg);
    }

    // Replace the entire scope bytes (padded with zeros if smaller than CB)
    void SetScopeBytes(TfxExtern id, const void* src, size_t numBytes) {
        Scope& s = scopes[id];
        if (!s.cpu.empty()) {
            const size_t n = std::min(numBytes, s.cpu.size());
            if (n && src) std::memcpy(s.cpu.data(), src, n);
            if (n < s.cpu.size()) std::memset(s.cpu.data() + n, 0, s.cpu.size() - n);
        }
        else {
            s.cpu.assign(numBytes, 0u);
            if (src && numBytes) std::memcpy(s.cpu.data(), src, numBytes);
        }
        s.dirty = true;
    }

    ID3D11Buffer* GetBuffer(TfxExtern id) const {
        auto it = scopes.find(id); return (it == scopes.end()) ? nullptr : it->second.gpu.Get();
    }

    void BindPS(ID3D11DeviceContext* ctx, UINT slot, TfxExtern id) const {
        ID3D11Buffer* b = GetBuffer(id);
        ctx->PSSetConstantBuffers(slot, 1, &b);
    }
    void BindVS(ID3D11DeviceContext* ctx, UINT slot, TfxExtern id) const {
        ID3D11Buffer* b = GetBuffer(id);
        ctx->VSSetConstantBuffers(slot, 1, &b);
    }



    // -------- convenience seeders --------
    static ExternStorage FilledDefaults() {
        ExternStorage ex;
        ex.set(TfxExtern::Frame, MakeScopeFrameDefaults());
        ex.set(TfxExtern::View, ViewExtern{});
        ex.set(TfxExtern::Deferred, DeferredExtern{});
        ex.set(TfxExtern::ShadowMask, MakeShadowMaskAllOnes());
        ex.set(TfxExtern::GlobalLighting, MakeGlobalLightingDefaults());
        ex.set(TfxExtern::Fxaa, FxaaExtern{});
        ex.set(TfxExtern::Atmosphere, AtmosphereExtern{}); // <— new
        ex.set(TfxExtern::SimpleGeometry, SimpleGeometryExtern{}); // << add this
        ex.set(TfxExtern::DeferredLight, DeferredLightExtern{});
        ex.set(TfxExtern::Transparent, TransparentExtern{});
        return ex;
    }

    void SetFrameTimes(float game_time, float delta_game_time, float exposure_scale, float render_time = 0.0f) {
        FrameExtern f{};
        if (auto it = scopes.find(TfxExtern::Frame);
            it != scopes.end() && it->second.cpu.size() >= sizeof(FrameExtern))
        {
            std::memcpy(&f, it->second.cpu.data(), sizeof(FrameExtern));
        }
        f.game_time = game_time;
        f.render_time = game_time;
        f.delta_game_time = delta_game_time;
        f.exposure_time = game_time;
        //f.exposure_scale = exposure_scale;
        set(TfxExtern::Frame, f);
    }

    void SetViewProjectiveToCamera(View& v, D3D11_VIEWPORT& vp)
    {
        v.resolution_width = vp.Width;
        v.resolution_height = vp.Height;

        // Base inverses.
        v.camera_to_world = MInverse(v.world_to_camera);
        v.projective_to_camera = MInverse(v.camera_to_projective);

        // Row-major + row-vectors: compose left->right in the order the spaces are visited.
        // world -> projective: (world->camera) then (camera->projective)
        v.world_to_projective = MMul(v.world_to_camera, v.camera_to_projective);

        // projective -> world: (projective->camera) then (camera->world)
        v.projective_to_world = MMul(v.projective_to_camera, v.camera_to_world);

        // Pixel-center -> Projective (D3D top-left origin).
        const XMFLOAT4X4 tptop = TargetPixelToProjective(vp.Width, vp.Height);

        // pixel -> camera: pixel->projective then projective->camera
        v.target_pixel_to_camera = MMul(tptop, v.projective_to_camera);

        // pixel -> world: (pixel->camera) then (camera->world)
        v.target_pixel_to_world = MMul(v.target_pixel_to_camera, v.camera_to_world);

        // Camera position (translation is in the last row for row-major)
        v.position = WAxis(v.camera_to_world);

        // Whatever this is used for; keep it but with a sane world_to_projective.
        const XMFLOAT4 wproj = WAxis(v.world_to_projective);
        const XMFLOAT4 z = VecZ();
        v.unk30 = { z.x - wproj.x, z.y - wproj.y, z.z - wproj.z, z.w - wproj.w };

        // Pixel -> world direction without world translation.
        const XMFLOAT4X4 ctow_noT = RemoveTranslation(v.camera_to_world);
        // pixel->proj -> cam -> (cam->world without T)
        v.tptow_no_proj_w = MMul(MMul(tptop, v.projective_to_camera), ctow_noT);

        set(TfxExtern::View, v);
    }

    void SetGlobalLighting(const GlobalLightingExtern& g) { set(TfxExtern::GlobalLighting, g); }

    GlobalLightingExtern GetGlobalLighting() const {
        GlobalLightingExtern g{};
        if (auto it = scopes.find(TfxExtern::GlobalLighting);
            it != scopes.end() && it->second.cpu.size() >= sizeof(GlobalLightingExtern))
        {
            std::memcpy(&g, it->second.cpu.data(), sizeof(GlobalLightingExtern));
        }
        return g;
    }

    void SetFxaa(float noise_time, const Vec4* intensity = nullptr) {
        FxaaExtern fx{};
        if (auto it = scopes.find(TfxExtern::Fxaa);
            it != scopes.end() && it->second.cpu.size() >= sizeof(FxaaExtern))
        {
            std::memcpy(&fx, it->second.cpu.data(), sizeof(FxaaExtern));
        }
        fx.noise_time = noise_time;
        if (intensity) fx.noise_intensity_scale = *intensity;
        set(TfxExtern::Fxaa, fx);
    }
    // Set every SRV (pass nullptr for any you don’t have)
    void SetTransparentSRVs(ExternStorage& ex,
        ID3D11ShaderResourceView* atmos_ss_far_lookup,
        ID3D11ShaderResourceView* atmos_ss_far_lookup_downsampled,
        ID3D11ShaderResourceView* atmos_ss_near_lookup,
        ID3D11ShaderResourceView* atmos_ss_near_lookup_downsampled,
        ID3D11ShaderResourceView* atmosphere_depth_angle_density_lookup,
        ID3D11ShaderResourceView* unk28,
        ID3D11ShaderResourceView* unk30,
        ID3D11ShaderResourceView* unk38,
        ID3D11ShaderResourceView* unk40,
        ID3D11ShaderResourceView* unk48,
        ID3D11ShaderResourceView* unk50,
        ID3D11ShaderResourceView* unk58,
        ID3D11ShaderResourceView* unk60
    ) {
        TransparentExtern t = MakeTransparentExternDefaults();
        t.atmos_ss_far_lookup = atmos_ss_far_lookup;
        t.atmos_ss_far_lookup_downsampled = atmos_ss_far_lookup_downsampled;
        t.atmos_ss_near_lookup = atmos_ss_near_lookup;
        t.atmos_ss_near_lookup_downsampled = atmos_ss_near_lookup_downsampled;
        t.atmosphere_depth_angle_density_lookup = atmosphere_depth_angle_density_lookup;
        t.unk28 = unk28; t.unk30 = unk30; t.unk38 = unk38;
        t.unk40 = unk40; t.unk48 = unk48; t.unk50 = unk50;
        t.unk58 = unk58; t.unk60 = unk60;

        ex.set(TfxExtern::Transparent, t);
    }

private:
    // Helper: read current GPU width (returns 0 if unknown)
    static uint32_t GetByteWidth(ID3D11Buffer* buf) {
        if (!buf) return 0u;
        D3D11_BUFFER_DESC bd{};
        buf->GetDesc(&bd);
        return bd.ByteWidth;
    }
};





