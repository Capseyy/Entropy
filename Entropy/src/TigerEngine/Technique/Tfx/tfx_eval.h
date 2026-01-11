#pragma once
#include "tfx.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include "extern.h"
#include "global_channel_usage.h"
#include <cmath>
#define _USE_MATH_DEFINES
#include <numbers>
#ifndef TFX_EVAL_HELPERS_DEFINED
#define TFX_EVAL_HELPERS_DEFINED
#include "Runtime/Assets/Technique.h"

#include <cstdarg>

struct TfxTraceSink {
    std::string* out = nullptr;
};

inline void TfxTraceV(TfxTraceSink* sink, FILE* fallback, const char* fmt, va_list args)
{
    if (sink && sink->out) {
        char buf[2048];
        va_list args2;
        va_copy(args2, args);
        int n = std::vsnprintf(buf, sizeof(buf), fmt, args2);
        va_end(args2);
        if (n > 0) sink->out->append(buf, buf + (n < (int)sizeof(buf) ? n : (int)sizeof(buf) - 1));
    } else {
        std::vfprintf(fallback, fmt, args);
    }
}

inline void TfxTrace(TfxTraceSink* sink, FILE* fallback, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    TfxTraceV(sink, fallback, fmt, args);
    va_end(args);
}

namespace tfx_eval_detail {

    Vec4 GetCameraFloatTfx();

    inline Vec4 abs4(const Vec4& a) { return Vec4(std::fabs(a.x), std::fabs(a.y), std::fabs(a.z), std::fabs(a.w)); }

    inline Vec4 round4(const Vec4& a) { return Vec4(std::round(a.x), std::round(a.y), std::round(a.z), std::round(a.w)); }

    inline Vec4 wrap_to_half(const Vec4& a) { 
        return a - round4(a);
    }

    inline Vec4 sin_rot_est_clamped(const Vec4& a) {

        Vec4 y = a * (abs4(a) * Vec4::splat(-16.0f) + Vec4::splat(8.0f));
        return y * (abs4(y) * Vec4::splat(0.225f) + Vec4::splat(0.775f));
    }
    inline Vec4 sin_rot_est(const Vec4& a) { return sin_rot_est_clamped(wrap_to_half(a)); }
    inline Vec4 cos_rot_est(const Vec4& a) { return sin_rot_est(a + Vec4(0.25f, 0.25f, 0.25f, 0.25f)); }
    inline Vec4 sin_cos_rot_est(const Vec4& a) {

        return sin_rot_est(a + Vec4(0.0f, 0.25f, 0.0f, 0.25f));
    }


    inline float hsum4(const Vec4& v) { return v.x + v.y + v.z + v.w; }


    inline Vec4 yzww(const Vec4& v) { return Vec4(v.y, v.z, v.w, v.w); }


    inline Vec4 triangle4(const Vec4& x) { return abs4(wrap_to_half(x)) * Vec4::splat(2.0f); }



    inline float hermite_smooth(float v) {
        float v2 = v * v;
        return (-2.0f * v + 3.0f) * v2;
    }


    inline Vec4 jitter4(const Vec4& x) {
        Vec4 rotations = Vec4::splat(x.x) * Vec4(4.67f, 2.99f, 1.08f, 1.35f)
            + Vec4(0.52f, 0.37f, 0.16f, 0.79f);
        Vec4 a = wrap_to_half(rotations);
        Vec4 ma = abs4(a) * Vec4::splat(-16.0f) + Vec4::splat(8.0f);
        Vec4 sa = a * Vec4::splat(0.25f);
        float v = hsum4(sa * ma) + 0.5f;
        return Vec4::splat(hermite_smooth(v));
    }

    inline Vec4 wander4(const Vec4& x) {
        Vec4 rot0 = Vec4::splat(x.x) * Vec4(4.08f, 1.02f, 3.0f / 5.37f, 3.0f / 9.67f)
            + Vec4(0.92f, 0.33f, 0.26f, 0.54f);
        Vec4 rot1 = Vec4::splat(x.x) * Vec4(1.83f, 3.09f, 0.39f, 0.87f)
            + Vec4(0.12f, 0.37f, 0.16f, 0.79f);
        Vec4 s0 = sin_rot_est(rot0);
        Vec4 s1 = sin_rot_est(rot1) * Vec4(0.02f, 0.02f, 0.28f, 0.28f);
        return Vec4::splat(0.5f + hsum4(s0 * s1));
    }

    inline Vec4 rand4(const Vec4& x) {
        float v0 = std::floor(x.x);

        Vec4 primes(1.0f / 1.043501f, 1.0f / 0.794471f, 1.0f / 0.113777f, 1.0f / 0.015101f);
        float val0 = std::fmod(v0 * hsum4(primes), 1.0f);
        val0 = std::fmod(val0 * val0 * 251.0f, 1.0f);
        return Vec4::splat(val0);
    }


    inline Vec4 rand_smooth4(const Vec4& x) {
        float v = x.x;
        float v0 = std::round(v);
        float v1 = v0 + 1.0f;
        float f = v - v0;
        float smooth_f = hermite_smooth(f);

        Vec4 primes(1.0f / 1.043501f, 1.0f / 0.794471f, 1.0f / 0.113777f, 1.0f / 0.015101f);
        auto rnd = [&](float base)->float {
            float t = std::fmod(base * hsum4(primes), 1.0f);
            t = std::fmod(t * t * 251.0f, 1.0f);
            return t;
            };
        float a = rnd(v0), b = rnd(v1);
        return Vec4::splat(a + (b - a) * smooth_f);
    }


    inline std::string to_str(const Vec4& v) {
        char b[128];
        std::snprintf(b, sizeof(b), "float4(%g,%g,%g,%g)", v.x, v.y, v.z, v.w);
        return b;
    }

    inline Vec4 clamp4(const Vec4& v, const Vec4& lo, const Vec4& hi) {
        return Vec4(
            std::min(std::max(v.x, lo.x), hi.x),
            std::min(std::max(v.y, lo.y), hi.y),
            std::min(std::max(v.z, lo.z), hi.z),
            std::min(std::max(v.w, lo.w), hi.w));
    }

    inline Vec4 frac4(const Vec4& a) {
        auto f = [](float x) { return x - std::floor(x); };
        return Vec4(f(a.x), f(a.y), f(a.z), f(a.w));
    }
    inline Vec4 saturate4(const Vec4& a) { return clamp4(a, Vec4::zero(), Vec4::one()); }

    inline Vec4 floor4(const Vec4& a) { return Vec4(std::floor(a.x), std::floor(a.y), std::floor(a.z), std::floor(a.w)); }
    inline Vec4 ceil4(const Vec4& a) { return Vec4(std::ceil(a.x), std::ceil(a.y), std::ceil(a.z), std::ceil(a.w)); }

    inline Vec4 sign4(const Vec4& a) {
        auto s = [](float x) { return (x > 0) - (x < 0); };
        return Vec4((float)s(a.x), (float)s(a.y), (float)s(a.z), (float)s(a.w));
    }
    inline Vec4 min4(const Vec4& a, const Vec4& b) {
        return Vec4(std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z), std::min(a.w, b.w));
    }
    inline Vec4 max4(const Vec4& a, const Vec4& b) {
        return Vec4(std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z), std::max(a.w, b.w));
    }
    inline Vec4 less_than_mask(const Vec4& a, const Vec4& b) {
        return Vec4(a.x < b.x ? 1.f : 0.f,
            a.y < b.y ? 1.f : 0.f,
            a.z < b.z ? 1.f : 0.f,
            a.w < b.w ? 1.f : 0.f);
    }
    inline Vec4 is_zero_mask(const Vec4& a) {
        return Vec4(a.x == 0.f ? 1.f : 0.f,
            a.y == 0.f ? 1.f : 0.f,
            a.z == 0.f ? 1.f : 0.f,
            a.w == 0.f ? 1.f : 0.f);
    }
    inline Vec4 dot_splat(const Vec4& a, const Vec4& b) {
        float d = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
        return Vec4::splat(d);
    }
    inline Vec4 swizzle_fields(const Vec4& v, uint8_t fields) {
        const float c[4] = { v.x, v.y, v.z, v.w };
        return Vec4(
            c[(fields >> 6) & 3],
            c[(fields >> 4) & 3],
            c[(fields >> 2) & 3],
            c[(fields >> 0) & 3]);
    }

    inline Vec4 mul_vec4_mat(const Vec4& v, const Mat4& m) {
        return Vec4(
            v.x * m.x_axis.x + v.y * m.y_axis.x + v.z * m.z_axis.x + v.w * m.w_axis.x,
            v.x * m.x_axis.y + v.y * m.y_axis.y + v.z * m.z_axis.y + v.w * m.w_axis.y,
            v.x * m.x_axis.z + v.y * m.y_axis.z + v.z * m.z_axis.z + v.w * m.w_axis.z,
            v.x * m.x_axis.w + v.y * m.y_axis.w + v.z * m.z_axis.w + v.w * m.w_axis.w);
    }
    inline Vec4 step4(const Vec4& edge, const Vec4& x) {
        return Vec4(
            x.x >= edge.x ? 1.f : 0.f,
            x.y >= edge.y ? 1.f : 0.f,
            x.z >= edge.z ? 1.f : 0.f,
            x.w >= edge.w ? 1.f : 0.f
        );
    }

    
    inline Vec4 xor01(const Vec4& a, const Vec4& b) {
        return Vec4(
            a.x + b.x - 2.f * a.x * b.x,
            a.y + b.y - 2.f * a.y * b.y,
            a.z + b.z - 2.f * a.z * b.z,
            a.w + b.w - 2.f * a.w * b.w
        );
    }






} 
#endif 

inline const char* OpName(TfxBytecode);

struct ShaderBindingState
{
    static constexpr uint32_t MaxStages = 8;
    static constexpr uint32_t MaxSlots = 32;

    std::array<std::array<std::shared_ptr<EntropyAssets::Texture2DRes>, MaxSlots>, MaxStages> textures{};
    std::array<std::array<uint32_t, MaxSlots>, MaxStages> samplers{};
    std::array<std::array<uint32_t, MaxSlots>, MaxStages> uavs{};  

    void setTexture(uint32_t stage, uint32_t slot, std::shared_ptr<EntropyAssets::Texture2DRes> tex)
    {
        if (stage < MaxStages && slot < MaxSlots) textures[stage][slot] = std::move(tex);
    }
    void setSampler(uint32_t stage, uint32_t slot, uint32_t samplerIndex)
    {
        if (stage < MaxStages && slot < MaxSlots) samplers[stage][slot] = samplerIndex;
    }
    void setUav(uint32_t stage, uint32_t slot, uint32_t uavIndex)
    {
        if (stage < MaxStages && slot < MaxSlots) uavs[stage][slot] = uavIndex;
    }
};



static inline std::string DescribePayload(const TfxData& i) {
    char b[64]; b[0] = 0;
    switch (i.op) {
    case TfxBytecode::PushConstantVec4: {
        auto d = std::get<PushConstantVec4Data>(i.data);
        std::snprintf(b, sizeof(b), "const=%u", d.constant_index); break;
    }
    case TfxBytecode::LerpConstant:
    case TfxBytecode::LerpConstantSaturated: {
        auto d = std::get<LerpConstantData>(i.data);
        std::snprintf(b, sizeof(b), "start=%u", d.constant_start); break;
    }
    case TfxBytecode::PushExternInputFloat: {
        auto d = std::get<PushExternInputFloatData>(i.data);
        std::snprintf(b, sizeof(b), "ext=%u off=%u(float)", (unsigned)d.ext, d.offset); break;
    }
    case TfxBytecode::PushExternInputVec4: {
        auto d = std::get<PushExternInputVec4Data>(i.data);
        std::snprintf(b, sizeof(b), "ext=%u off=%u(vec4)", (unsigned)d.ext, d.offset); break;
    }
    case TfxBytecode::PushExternInputMat4: {
        auto d = std::get<PushExternInputMat4Data>(i.data);
        std::snprintf(b, sizeof(b), "ext=%u off=%u(mat4)", (unsigned)d.ext, d.offset); break;
    }
    case TfxBytecode::PushFromOutput: {
        auto d = std::get<PushFromOutputData>(i.data);
        std::snprintf(b, sizeof(b), "cb[%u]", d.element); break;
    }
    case TfxBytecode::PopOutput: {
        auto d = std::get<PopOutputData>(i.data);
        std::snprintf(b, sizeof(b), "cb[%u]", d.element); break;
    }
    case TfxBytecode::PopOutputMat4: {
        auto d = std::get<PopOutputMat4Data>(i.data);
        std::snprintf(b, sizeof(b), "cb[%u..%u]", d.element, d.element + 3); break;
    }
    case TfxBytecode::PushTemp: {
        auto d = std::get<PushTempData>(i.data);
        std::snprintf(b, sizeof(b), "t[%u]", d.slot); break;
    }
    case TfxBytecode::PopTemp: {
        auto d = std::get<PopTempData>(i.data);
        std::snprintf(b, sizeof(b), "t[%u]", d.slot); break;
    }
    case TfxBytecode::PushSampler: {
        auto d = std::get<PushSamplerData>(i.data);
        std::snprintf(b, sizeof(b), "index=%u", d.index); break;
    }
    case TfxBytecode::SetShaderTexture:
    case TfxBytecode::SetShaderSampler:
    case TfxBytecode::SetShaderUav: {
        auto d = std::get<SetShaderBindingData>(i.data);
        std::snprintf(b, sizeof(b), "stage=%u slot=%u", d.stage, d.slot); break;
    }
    case TfxBytecode::PushTexDimensions:
    case TfxBytecode::PushTexTileParams:
    case TfxBytecode::PushTexTileCount: {
        auto d = std::get<PushTexParamData>(i.data);
        std::snprintf(b, sizeof(b), "idx=%u fields=0x%02X", d.index, d.fields); break;
    }
    case TfxBytecode::Spline4Const:
    case TfxBytecode::Spline8Const:
    case TfxBytecode::Spline8ConstChain: {
        auto d = std::get<SplineConstData>(i.data);
        std::snprintf(b, sizeof(b), "start=%u", d.constant_start); break;
    }
    default: break;
    }
    return std::string(b);
}


inline void EvaluateExpressionEoF(const std::vector<TfxData>& ops,
    const ExternStorage& externs,
    std::vector<Vec4>& cb,
    const std::vector<Vec4>& constants,
    std::array<Vec4, 16>& temp,
    std::unordered_map<uint32_t, Vec4> channel_floats,
    std::vector<std::shared_ptr<EntropyAssets::Texture2DRes>> texs,
    uint32_t technique_id,
    ShaderBindingState* outBindings,
    TfxTraceSink* traceSink = nullptr,
    bool trace = true)
{
    using namespace tfx_eval_detail;

    std::vector<Vec4> stack; stack.reserve(64);
    auto push = [&](const Vec4& v) { stack.push_back(v); };
    auto pop1 = [&](size_t ip, const char* opname) -> Vec4 {
        if (stack.empty()) {
            if (trace) TfxTrace(traceSink, stderr, "[eval] stack underflow before %s at ip=%zu, returning 0\n", opname, ip);
            return Vec4::zero();
        }
        auto v = stack.back(); stack.pop_back(); return v;
        };

    auto pop_u32 = [&](size_t ip, const char* opname) -> uint32_t {
        Vec4 v = pop1(ip, opname);
        float fx = std::round(v.x);
        if (fx < 0.0f) fx = 0.0f;
        return (uint32_t)fx;
        };
    auto push_u32 = [&](uint32_t u) { push(Vec4::splat((float)u)); };

    auto popN_discard = [&](size_t ip, const char* opname, size_t n) {
        for (size_t k = 0; k < n; ++k) {
            (void)pop1(ip, opname);
        }
        };

    Mat4 cachedM{};
    std::vector<std::shared_ptr<EntropyAssets::Texture2DRes>> texs_eval;
    for (size_t ip = 0; ip < ops.size(); ++ip) {
        const auto& i = ops[ip];
        if (trace) {
            const auto payload = DescribePayload(i);
            TfxTrace(traceSink, stdout,"ip=%04zu %-22s %s  (stack=%zu)\n",
                ip, OpName(i.op), payload.empty() ? "" : payload.c_str(), stack.size());
        }

        switch (i.op) {
            
        case TfxBytecode::Add:
        case TfxBytecode::Add2: { auto b = pop1(ip, "Add"), a = pop1(ip, "Add"); push(a + b); break; }
        case TfxBytecode::Subtract: { auto b = pop1(ip, "Sub"), a = pop1(ip, "Sub"); push(a - b); break; }
        case TfxBytecode::Multiply:
        case TfxBytecode::Multiply2: { auto b = pop1(ip, "Mul"), a = pop1(ip, "Mul"); push(a * b); break; }
        case TfxBytecode::Divide: { auto b = pop1(ip, "Div"), a = pop1(ip, "Div"); push(a / b); break; }
        case TfxBytecode::Min: { auto b = pop1(ip, "Min"), a = pop1(ip, "Min"); push(min4(a, b)); break; }
        case TfxBytecode::Max: { auto b = pop1(ip, "Max"), a = pop1(ip, "Max"); push(max4(a, b)); break; }
        case TfxBytecode::IsZero: { auto a = pop1(ip, "IsZero"); push(is_zero_mask(a)); break; }
        case TfxBytecode::LessThan: { auto b = pop1(ip, "LT"), a = pop1(ip, "LT"); push(less_than_mask(b, a)); break; }
        case TfxBytecode::Dot: { auto b = pop1(ip, "Dot"), a = pop1(ip, "Dot"); push(dot_splat(a, b)); break; }

        case TfxBytecode::Frac: { auto a = pop1(ip, "Frac");       push(frac4(a));     break; }
        case TfxBytecode::Saturate: { auto a = pop1(ip, "Saturate");   push(saturate4(a)); break; }
        case TfxBytecode::Abs: { auto a = pop1(ip, "Abs");        push(abs4(a));      break; }
        case TfxBytecode::Negate: { auto a = pop1(ip, "Neg");        push(Vec4::zero() - a); break; }
        case TfxBytecode::Floor: { auto a = pop1(ip, "Floor");      push(floor4(a));    break; }
        case TfxBytecode::Ceil: { auto a = pop1(ip, "Ceil");       push(ceil4(a));     break; }
        case TfxBytecode::Round: { auto a = pop1(ip, "Round");      push(round4(a));    break; }
        case TfxBytecode::Sign: { auto a = pop1(ip, "Sign");       push(sign4(a));     break; }

        case TfxBytecode::Merge_1_3: { auto t0 = pop1(ip, "M13"); auto t1 = pop1(ip, "M13"); push(Vec4(t1.x, t0.x, t0.y, t0.z)); break; }
        case TfxBytecode::Merge_2_2: { auto t0 = pop1(ip, "M22"); auto t1 = pop1(ip, "M22"); push(Vec4(t1.x, t1.y, t0.x, t0.y)); break; }
        case TfxBytecode::Merge_3_1: { auto t0 = pop1(ip, "M31"); auto t1 = pop1(ip, "M31"); push(Vec4(t1.x, t1.y, t1.z, t0.x)); break; }

        case TfxBytecode::Lerp: {
            auto t = pop1(ip, "Lerp"); auto b = pop1(ip, "Lerp"); auto a = pop1(ip, "Lerp");
            push(a + (b - a) * t); break;
        }
        case TfxBytecode::LerpSaturated: {
            auto t = pop1(ip, "LerpSat"); auto b = pop1(ip, "LerpSat"); auto a = pop1(ip, "LerpSat");
            push(saturate4(a + (b - a) * t)); break;
        }
        case TfxBytecode::MultiplyAdd: {
            auto t0 = pop1(ip, "MAD"); auto t1 = pop1(ip, "MAD"); auto t2 = pop1(ip, "MAD");
            push(t1 * t2 + t0); break;
        }
        case TfxBytecode::Clamp: {
            auto hi = pop1(ip, "Clamp"), lo = pop1(ip, "Clamp"), v = pop1(ip, "Clamp");
            push(clamp4(v, lo, hi)); break;
        }

        case TfxBytecode::PermuteAllX: { auto a = pop1(ip, "PermXXXX"); push(Vec4(a.x, a.x, a.x, a.x)); break; }
        case TfxBytecode::Permute: { auto d = std::get<PermuteData>(i.data); auto a = pop1(ip, "Perm"); push(swizzle_fields(a, d.fields)); break; }

                                 
        case TfxBytecode::PushConstantVec4: {
            auto d = std::get<PushConstantVec4Data>(i.data);
            Vec4 v = (d.constant_index < constants.size()) ? constants[d.constant_index] : Vec4::zero();
            push(v); break;
        }
        case TfxBytecode::LerpConstant:
        case TfxBytecode::LerpConstantSaturated: {
            auto d = std::get<LerpConstantData>(i.data);
            auto t = pop1(ip, "LerpC");
            size_t i0 = d.constant_start, i1 = d.constant_start + 1;
            Vec4 a = (i0 < constants.size()) ? constants[i0] : Vec4::zero();
            Vec4 b = (i1 < constants.size()) ? constants[i1] : Vec4::zero();
            Vec4 r = a + (b - a) * t;
            if (i.op == TfxBytecode::LerpConstantSaturated) r = saturate4(r);
            push(r); break;
        }
        case TfxBytecode::PushExternInputFloat: {
            auto d = std::get<PushExternInputFloatData>(i.data);
            float f = externs.getFloat(d.ext, size_t(d.offset) * 4);
            if (trace) {
                TfxTrace(traceSink, stdout,"    PushExternInputFloat: ext=%u offset=%u -> %g\n", (unsigned)d.ext, d.offset, f);
            }
            push(Vec4::splat(f)); break;
        }
        case TfxBytecode::PushExternInputVec4: {
            auto d = std::get<PushExternInputVec4Data>(i.data);
            
            push(externs.getVec4(d.ext, size_t(d.offset) * 16));
            break;
        }

        case TfxBytecode::PushExternInputMat4: {
            auto d = std::get<PushExternInputMat4Data>(i.data);
            
            Mat4 m = externs.getMat4(d.ext, size_t(d.offset) * 16);

            if (trace) {
                PrintMat4("PushExternInputMat4", m, size_t(d.offset) * 16);
            }

            push(m.x_axis);
            push(m.y_axis);
            push(m.z_axis);
            push(m.w_axis);

            break;
        }
        case TfxBytecode::TransformVec4: {
            Vec4 value = pop1(ip, "TransformVec4 value");
            Vec4 w_axis = pop1(ip, "TransformVec4 w");
            Vec4 z_axis = pop1(ip, "TransformVec4 z");
            Vec4 y_axis = pop1(ip, "TransformVec4 y");
            Vec4 x_axis = pop1(ip, "TransformVec4 x");

            Mat4 mat{};
            mat.x_axis = x_axis;
            mat.y_axis = y_axis;
            mat.z_axis = z_axis;
            mat.w_axis = w_axis;

            push(mul_vec4_mat(value, mat));
            break;
        }

                                       
        case TfxBytecode::PushGlobalChannelVector: {
            const auto d = std::get<PushGlobalChannelVectorData>(i.data);
            
            tfx::MarkGlobalChannelUsed(d.unk1);

            if (trace) {
                TfxTrace(traceSink, stdout,"    PushGlobalChannelVector: channel_id=%u\n", d.unk1);
			}
            Vec4 v = externs.getVec4(TfxExtern::Generic, size_t(d.unk1) * 16);
            push(v);
            break;
        }
        case TfxBytecode::PushObjectChannelVector: {
            const auto& d = std::get_if<PushObjectChannelVectorData>(&i.data);
            if (!channel_floats.empty()) {
                
                const auto it = channel_floats.find(d->hash_be);
                const Vec4 float_value = (it != channel_floats.end()) ? it->second : Vec4::splat(1.0f);
                
                push(float_value);
            }
            else {
                
                push(Vec4::splat(1.0f));
            }
            if (trace) {
                TfxTrace(traceSink, stdout,"    PushObjectChannelVector: hash_be=0x%08X\n", d->hash_be);
			}


            break;
        }
        case TfxBytecode::PushTexDimensions:
        {
            const auto d = std::get<PushTexParamData>(i.data);

            Vec4 dims = Vec4(0.25f, 0.25f, 0.25f, 0.0625f);

            const Vec4 v = swizzle_fields(dims, d.fields);


            

            if (trace) {
                TfxTrace(traceSink, stdout,"    PushTexDimensions: idx=%u fields=0x%02X -> %s\n",
                    d.index, d.fields, to_str(v).c_str());
			}
            push(v);
            break;
        }

        
        case TfxBytecode::PushTemp: { auto d = std::get<PushTempData>(i.data); push(temp[d.slot]); break; }
        case TfxBytecode::PopTemp: { auto d = std::get<PopTempData>(i.data);  temp[d.slot] = pop1(ip, "PopTemp"); break; }
        case TfxBytecode::PushFromOutput: {
            auto d = std::get<PushFromOutputData>(i.data);
            push(d.element < cb.size() ? cb[d.element] : Vec4::zero()); break;
        }
        case TfxBytecode::PopOutput: {
            auto d = std::get<PopOutputData>(i.data);
            if (d.element >= cb.size()) cb.resize(d.element + 1, Vec4::zero());
            Vec4 v = pop1(ip, "PopOutput");
            if (trace) TfxTrace(traceSink, stdout,"    -> cb[%u] = %s\n", d.element, to_str(v).c_str());
            cb[d.element] = v; break;
        }

        case TfxBytecode::Spline8Const: {
            auto d = std::get<SplineConstData>(i.data);
            
            Vec4 X = pop1(ip, "Spline8Const");

            
            auto load = [&](size_t rel) -> Vec4 {
                size_t k = size_t(d.constant_start) + rel;
                return (k < constants.size()) ? constants[k] : Vec4::zero();
                };
            Vec4 C3 = load(0), C2 = load(1), C1 = load(2), C0 = load(3);
            Vec4 D3 = load(4), D2 = load(5), D1 = load(6), D0 = load(7);
            Vec4 Cth = load(8), Dth = load(9);

            
            Vec4 X2 = X * X;
            Vec4 Chigh = C3 * X + C2;
            Vec4 Clow = C1 * X + C0;
            Vec4 Ceval = Chigh * X2 + Clow;

            Vec4 Dhigh = D3 * X + D2;
            Vec4 Dlow = D1 * X + D0;
            Vec4 Deval = Dhigh * X2 + Dlow;

            
            Vec4 Cmask = step4(Cth, X);
            Vec4 Dmask = step4(Dth, X);

            Vec4 Cchan = Vec4(xor01(Cmask, yzww(Cmask)).x,
                xor01(Cmask, yzww(Cmask)).y,
                xor01(Cmask, yzww(Cmask)).z,
                Cmask.w); 
            Vec4 Dchan = Vec4(xor01(Dmask, yzww(Dmask)).x,
                xor01(Dmask, yzww(Dmask)).y,
                xor01(Dmask, yzww(Dmask)).z,
                Dmask.w);

            
            float Csum = hsum4(Ceval * Cchan);
            float Dsum = hsum4(Deval * Dchan);

            float spline = (Dmask.x > 0.f) ? Dsum : Csum;
            Vec4 result = Vec4::splat(spline);

            if (trace) {
                TfxTrace(traceSink, stdout,"    Spline8: Csum=%g Dsum=%g use=%s\n",
                    Csum, Dsum, (Dmask.x > 0.f ? "D" : "C"));
            }

            push(result);
            break;
        }
                                      
        case TfxBytecode::Spline4Const: {
            auto d = std::get<SplineConstData>(i.data);
            Vec4 X = pop1(ip, "Spline4Const");

            auto load = [&](size_t rel)->Vec4 {
                size_t k = size_t(d.constant_start) + rel;
                return (k < constants.size()) ? constants[k] : Vec4::zero();
                };

            Vec4 C3 = load(0), C2 = load(1), C1 = load(2), C0 = load(3);
            Vec4 Th = load(4);

            Vec4 X2 = X * X;
            Vec4 high = C3 * X + C2;
            Vec4 low = C1 * X + C0;
            Vec4 eval = high * X2 + low;

            Vec4 tmask = step4(Th, X);
            Vec4 xorm = xor01(tmask, yzww(tmask));
            Vec4 chan = Vec4(xorm.x, xorm.y, xorm.z, tmask.w);

            float sum = hsum4(eval * chan);
            if (trace) TfxTrace(traceSink, stdout,"    Spline4: sum=%g\n", sum);
            push(Vec4::splat(sum));
            break;
        }

                                      
        case TfxBytecode::Spline8ConstChain: {
            auto d = std::get<SplineConstData>(i.data);

            Vec4 Rec = pop1(ip, "Spline8Chain Rec");
            Vec4 X = pop1(ip, "Spline8Chain X");

            auto load = [&](size_t rel)->Vec4 {
                size_t k = size_t(d.constant_start) + rel;
                return (k < constants.size()) ? constants[k] : Vec4::zero();
                };

            Vec4 C3 = load(0), C2 = load(1), C1 = load(2), C0 = load(3);
            Vec4 D3 = load(4), D2 = load(5), D1 = load(6), D0 = load(7);
            Vec4 Cth = load(8), Dth = load(9);

            Vec4 X2 = X * X;

            Vec4 Chigh = C3 * X + C2, Clow = C1 * X + C0;
            Vec4 Ceval = Chigh * X2 + Clow;

            Vec4 Dhigh = D3 * X + D2, Dlow = D1 * X + D0;
            Vec4 Deval = Dhigh * X2 + Dlow;

            Vec4 Cmask = step4(Cth, X);
            Vec4 Dmask = step4(Dth, X);

            Vec4 Cchan = Vec4(xor01(Cmask, yzww(Cmask)).x,
                xor01(Cmask, yzww(Cmask)).y,
                xor01(Cmask, yzww(Cmask)).z,
                Cmask.w);
            Vec4 Dchan = Vec4(xor01(Dmask, yzww(Dmask)).x,
                xor01(Dmask, yzww(Dmask)).y,
                xor01(Dmask, yzww(Dmask)).z,
                Dmask.w);

            float Csum = hsum4(Ceval * Cchan);
            float Dsum = hsum4(Deval * Dchan);

            float inter = (Cmask.x > 0.f) ? Csum : Rec.x;
            float spline = (Dmask.x > 0.f) ? Dsum : inter;

            if (trace) {
                TfxTrace(traceSink, stdout,"    Spline8Chain: Csum=%g Dsum=%g inter=%g use=%s\n",
                    Csum, Dsum, inter, (Dmask.x > 0.f ? "D" : (Cmask.x > 0.f ? "C" : "Rec")));
            }

            push(Vec4::splat(spline));
            break;
        }

                                           
        case TfxBytecode::Gradient4Const: {
            auto d = std::get<GradientConstData>(i.data); 
            Vec4 X = pop1(ip, "Gradient4Const");

            auto load = [&](size_t rel)->Vec4 {
                size_t k = size_t(d.constant_start) + rel;
                return (k < constants.size()) ? constants[k] : Vec4::zero();
                };

            Vec4 base = load(0);
            Vec4 cred = load(1);
            Vec4 cgreen = load(2);
            Vec4 cblue = load(3);
            Vec4 calpha = load(4);
            Vec4 Th = load(5);

            
            Vec4 seg = Vec4(Th.y, Th.z, Th.w, 1.0f) - Th;
            Vec4 off = X - Th;

            
            auto safe_div = [&](float num, float den)->float {
                if (std::fabs(den) > 1e-6f) return num / den;
                
                return (num >= 0.0f) ? 1.0f : 0.0f;
                };

            Vec4 div(
                safe_div(off.x, seg.x),
                safe_div(off.y, seg.y),
                safe_div(off.z, seg.z),
                safe_div(off.w, seg.w)
            );
            Vec4 pct = saturate4(div);

            
            Vec4 Xinfl = cred * pct;
            Vec4 Yinfl = cgreen * pct;
            Vec4 Zinfl = cblue * pct;
            Vec4 Winfl = calpha * pct;

            Vec4 ones = Vec4::one();
            Vec4 added(
                hsum4(Xinfl * ones),
                hsum4(Yinfl * ones),
                hsum4(Zinfl * ones),
                hsum4(Winfl * ones)
            );

            Vec4 result = base + added;
            push(result);
            break;
        }

                                        
        case TfxBytecode::Triangle: {
            auto a = pop1(ip, "Triangle");
            push(triangle4(a));
            break;
        }
        case TfxBytecode::Jitter: {
            auto a = pop1(ip, "Jitter");
            push(jitter4(a));
            break;
        }
        case TfxBytecode::Wander: {
            auto a = pop1(ip, "Wander");
            push(wander4(a));
            break;
        }
        case TfxBytecode::Rand: {
            auto a = pop1(ip, "Rand");
            push(rand4(a));
            break;
        }
        case TfxBytecode::RandSmooth: {
            auto a = pop1(ip, "RandSmooth");
            push(rand_smooth4(a));
            break;
        }

                                    
        case TfxBytecode::VecRotSin: {
            auto a = pop1(ip, "VRSin");
            push(sin_rot_est(a));
            break;
        }
        case TfxBytecode::VecRotCos: {
            auto a = pop1(ip, "VRCos");
            push(cos_rot_est(a));
            break;
        }
        case TfxBytecode::VecRotSinCos: {
            auto a = pop1(ip, "VRSinCos");
            push(sin_cos_rot_est(a));
            break;
        }
        case TfxBytecode::Cubic: {

            auto coefficients = pop1(ip, "Cubic coeffs");
            auto x = pop1(ip, "Cubic x");

            Vec4 high = Vec4::splat(coefficients.x) * x
                + Vec4(coefficients.y, coefficients.y, coefficients.y, coefficients.y);
            Vec4 low = Vec4::splat(coefficients.z) * x
                + Vec4(coefficients.w, coefficients.w, coefficients.w, coefficients.w);

            Vec4 x2 = x * x;
            Vec4 out = high * x2 + low;

            push(out);
            break;
        }
        case TfxBytecode::PopOutputMat4: {
            auto d = std::get<PopOutputMat4Data>(i.data);
            if (d.element + 3 >= cb.size()) cb.resize(d.element + 4, Vec4::zero());
            Vec4 r3 = pop1(ip, "PopMat"), r2 = pop1(ip, "PopMat"), r1 = pop1(ip, "PopMat"), r0 = pop1(ip, "PopMat");
            if (trace) {
                TfxTrace(traceSink, stdout,"    -> cb[%u..%u] = %s, %s, %s, %s\n",
                    d.element, d.element + 3, to_str(r0).c_str(), to_str(r1).c_str(), to_str(r2).c_str(), to_str(r3).c_str());
            }
            cb[d.element + 0] = r0; cb[d.element + 1] = r1; cb[d.element + 2] = r2; cb[d.element + 3] = r3;
            break;
        }

                                       
        case TfxBytecode::PushSampler: {
            
            
            const auto d = std::get<PushSamplerData>(i.data);
            push_u32(d.index);
            if (trace) TfxTrace(traceSink, stdout,"    PushSampler idx=%u\n", d.index);
            break;
        }

        case TfxBytecode::SetShaderSampler: {
            const auto d = std::get<SetShaderBindingData>(i.data);
            const uint32_t idx = pop_u32(ip, "SetShaderSampler");
            if (outBindings) outBindings->setSampler(d.stage, d.slot, idx);
            if (trace) TfxTrace(traceSink, stdout,"    SetShaderSampler stage=%u slot=%u idx=%u\n", d.stage, d.slot, idx);
            break;
        }

        case TfxBytecode::SetShaderTexture: {
            auto d = std::get<SetShaderBindingData>(i.data);
            uint32_t idx = pop_u32(ip, "SetShaderTexture");

            std::shared_ptr<EntropyAssets::Texture2DRes> tex =
                (idx < texs_eval.size()) ? texs_eval[idx] : nullptr;

            if (outBindings) outBindings->setTexture(d.stage, d.slot, tex);

            if (trace) {
                TfxTrace(traceSink, stdout,"    SetShaderTexture stage=%u slot=%u idx=%u (%s)\n",
                    d.stage, d.slot, idx, tex ? "ok" : "null");
            }
            break;
        }

        case TfxBytecode::SetShaderUav: {
            
            const auto d = std::get<SetShaderBindingData>(i.data);
            const uint32_t idx = pop_u32(ip, "SetShaderUav");
            if (outBindings) outBindings->setUav(d.stage, d.slot, idx);
            if (trace) TfxTrace(traceSink, stdout,"    SetShaderUav stage=%u slot=%u idx=%u\n", d.stage, d.slot, idx);
            break;
        }

        case TfxBytecode::PushExternInputTextureView: {
            const auto d = std::get<PushExternInputTexData>(i.data);
            const size_t byte_off = size_t(d.offset) * 8; 

            ID3D11ShaderResourceView* srv = externs.getSRV(d.ext, byte_off);
            uint32_t idx = 0;
            bool found = false;

         
            if (srv) {
                for (uint32_t ti = 0; ti < (uint32_t)texs_eval.size(); ++ti) {
                    if (texs_eval[ti] && texs_eval[ti]->Get() == srv) { idx = ti; found = true; break; }
                }
                if (!found) {
                    auto wrap = std::make_shared<EntropyAssets::Texture2DRes>();
                    wrap->srv = srv; 
                    wrap->width = 1;
                    wrap->height = 1;
                    texs_eval.push_back(std::move(wrap));
                    idx = (uint32_t)texs_eval.size() - 1;
                }
            } else {
                
                idx = (uint32_t)texs_eval.size();
            }

            push_u32(idx);
            if (trace) {
                TfxTrace(traceSink, stdout,"    PushExternInputTextureView ext=%u off=%zu -> srv=%p idx=%u (%s)\n",
                    (unsigned)d.ext, byte_off, (void*)srv, idx, found ? "mapped" : "unmapped");
            }
            break;
        }

        case TfxBytecode::PushExternInputU32:
        case TfxBytecode::PushExternInputUav:
        case TfxBytecode::Unk49:
            break;
        case TfxBytecode::Unk4c:
            break;
        case TfxBytecode::Unk50:
            break;
        case TfxBytecode::Unk51:
            break;
        case TfxBytecode::PushTexTileParams:
        {
            const auto* d = std::get_if<PushTexParamData>(&i.data);
            if (!d) {
                if (trace) TfxTrace(traceSink, stderr,
                    "[eval] bad payload for PushTexTileParams\n");
                push(Vec4::zero());
                break;
            }

            Vec4 base(0.2f, 0.167f, 0.2f, 0.2f);


            Vec4 v = tfx_eval_detail::swizzle_fields(base, d->fields);

            if (trace) {
                TfxTrace(traceSink, stdout,"    PushTexTileParams: idx=%u fields=0x%02X -> %s\n",
                    d->index, d->fields, tfx_eval_detail::to_str(v).c_str());
            }

            push(v);
            break;
        }

        case TfxBytecode::PushTexTileCount:
        {
            const auto d = std::get<PushTexParamData>(i.data);

            Vec4 base(0.25f, 0.25f, 0.25f, 0.0625f);
            Vec4 v = swizzle_fields(base, d.fields);

            push(v);
            break;
        }
        case TfxBytecode::Unk42:
        case TfxBytecode::Unk55:
        case TfxBytecode::Unk56:
        case TfxBytecode::Unk57:
        case TfxBytecode::Unk58:
        case TfxBytecode::Unk1c:
        case TfxBytecode::Unk25:



        default:
            if (OpName(i.op) == "Unknown") {
                TfxTrace(traceSink, stderr, "[eval] unhandled opcode %s at ip=%zu in tech %08X\n", OpName(i.op), ip, technique_id);
            }

            break;
        }
    }
}

inline std::vector<uint32_t>
CollectObjectChannelU32(const std::vector<TfxData>& ops)
{
    std::vector<uint32_t> out;
    out.reserve(16);

    for (const auto& ins : ops) {
        if (ins.op != TfxBytecode::PushObjectChannelVector)
            continue;

        if (const auto* d = std::get_if<PushObjectChannelVectorData>(&ins.data)) {

            const uint32_t key = d->hash_be;

            out.push_back(key);
        }

    }

    return out;
}