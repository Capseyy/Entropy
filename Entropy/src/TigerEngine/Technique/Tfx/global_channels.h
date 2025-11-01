// global_channels.h
#pragma once
#include <array>
#include <limits>
#include "tfx_runtime.h"   // for Vec4
#include "extern.h"        // for ExternStorage, TfxExtern

enum class ChannelType : uint8_t {
    Float,
    Color,
    FloatRanged
};

struct GlobalChannel {
    const char* name = "";
    ChannelType type = ChannelType::Float;
    Vec4 value = Vec4::one();
    // For FloatRanged (x component is used, min/max are advisory/UI)
    float minv = 0.0f;
    float maxv = 1.0f;

    static GlobalChannel Make(const char* n, ChannelType t, const Vec4& v) {
        GlobalChannel g; g.name = n; g.type = t; g.value = v; return g;
    }
    static GlobalChannel MakeRanged(const char* n, float minv, float maxv, const Vec4& v) {
        GlobalChannel g; g.name = n; g.type = ChannelType::FloatRanged; g.value = v; g.minv = minv; g.maxv = maxv; return g;
    }
};

inline std::array<GlobalChannel, 256> GetGlobalChannelDefaults()
{
    std::array<GlobalChannel, 256> ch{};
    // defaults (everything zero + Float)
    for (auto& c : ch) c = GlobalChannel{};

    // --- Your mappings (1:1 with the Rust) ---

    ch[97].value = Vec4::zero();

    // Atmosphere
    ch[10].value = Vec4::one();
    ch[25].value = Vec4::splat(40.0f);
    ch[26].value = Vec4::splat(0.90f);        // atmos intensity?
    ch[35].value = Vec4::splat(0.55f);
    ch[40].value = Vec4::zero();
    ch[43].value = Vec4::zero();
    ch[68] = GlobalChannel::Make("unk68", ChannelType::Float, Vec4::splat(0.5f));
    ch[100].value = Vec4(0.41105f, 0.71309f, 0.56793f, 0.56793f);

    ch[75] = GlobalChannel::MakeRanged("unk75 (verity dark/light)", 0.0f, 1.0f, Vec4::zero());
    ch[76] = GlobalChannel::MakeRanged("unk76 (verity dark/light, cancels out unk75)", 0.0f, 1.0f, Vec4::zero());

    // Sun related
    ch[82].value = Vec4(1.f, 0.f, 0.f, 0.f);  // X controls AO in your note
    ch[83].value = Vec4::zero();
    ch[98].value = Vec4::zero();
    ch[100].value = Vec4::zero();             // NOTE: overrides the earlier value, as in your Rust

    ch[27] = GlobalChannel::Make("global specular intensity", ChannelType::Float, Vec4::one());
    ch[28] = GlobalChannel::Make("global specular tint", ChannelType::Color, Vec4::one());

    ch[31] = GlobalChannel::Make("global diffuse direct tint", ChannelType::Color, Vec4::one());
    ch[32] = GlobalChannel::Make("global diffuse direct intensity", ChannelType::Float, Vec4::one());
    ch[33] = GlobalChannel::Make("global diffuse penumbra tint", ChannelType::Color, Vec4::one());
    ch[34] = GlobalChannel::Make("global diffuse penumbra intensity", ChannelType::Float, Vec4::one());

    {
        const float INF = std::numeric_limits<float>::infinity();
        ch[37] = GlobalChannel::Make("fog start", ChannelType::Float, Vec4::splat(INF)); // Vec4::INFINITY
    }
    ch[41] = GlobalChannel::Make("fog falloff", ChannelType::Float, Vec4(50.f, 0.f, 0.f, 0.f)); // X * 50

    // Misc lights
    ch[84] = GlobalChannel::Make("ao intensity", ChannelType::Float, Vec4::one());

    ch[82].value = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
    ch[83].value = Vec4::zero();
    ch[127].value = Vec4::zero();

    // Line lights-ish
    ch[131].value = Vec4(0.0f, 0.5f, 0.3f, 0.0f);
    ch[138].value = Vec4::one();
    ch[139].value = Vec4::one();
    ch[140].value = Vec4::one();
    ch[145].value = Vec4::one();

    return ch;
}

// Flatten the 256 channels (values only) into an extern blob so
// PushGlobalChannelVector can read them via: getVec4(TfxExtern::Generic, index*16)
inline void PublishGlobalChannelsToExterns(ExternStorage& ex,
    const std::array<GlobalChannel, 256>& chans)
{
    // pack as contiguous Vec4[256]
    std::array<Vec4, 256> values{};
    for (size_t i = 0; i < 256; ++i) values[i] = chans[i].value;

    ex.set_raw(TfxExtern::Generic, values.data(), values.size() * sizeof(Vec4));
}
