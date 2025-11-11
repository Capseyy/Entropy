#pragma once
#include "tfx.h"
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <string>
#include "extern.h"
#include <cmath>
#define _USE_MATH_DEFINES
#include <numbers>
#ifndef TFX_EVAL_HELPERS_DEFINED
#define TFX_EVAL_HELPERS_DEFINED
namespace tfx_eval_detail {

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
    inline Vec4 abs4(const Vec4& a) { return Vec4(std::fabs(a.x), std::fabs(a.y), std::fabs(a.z), std::fabs(a.w)); }
    inline Vec4 floor4(const Vec4& a) { return Vec4(std::floor(a.x), std::floor(a.y), std::floor(a.z), std::floor(a.w)); }
    inline Vec4 ceil4(const Vec4& a) { return Vec4(std::ceil(a.x), std::ceil(a.y), std::ceil(a.z), std::ceil(a.w)); }
    inline Vec4 round4(const Vec4& a) { return Vec4(std::round(a.x), std::round(a.y), std::round(a.z), std::round(a.w)); }
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

} // namespace tfx_eval_detail
#endif 


inline Vec4 step4(const Vec4& edge, const Vec4& x) {
    return Vec4(
        x.x >= edge.x ? 1.f : 0.f,
        x.y >= edge.y ? 1.f : 0.f,
        x.z >= edge.z ? 1.f : 0.f,
        x.w >= edge.w ? 1.f : 0.f
    );
}

// XOR for {0,1} masks: a ? b = a + b - 2ab
inline Vec4 xor01(const Vec4& a, const Vec4& b) {
    return Vec4(
        a.x + b.x - 2.f * a.x * b.x,
        a.y + b.y - 2.f * a.y * b.y,
        a.z + b.z - 2.f * a.z * b.z,
        a.w + b.w - 2.f * a.w * b.w
    );
}

inline float hsum4(const Vec4& v) { return v.x + v.y + v.z + v.w; }

// Convenience swizzle used by the spline logic: .yzww
inline Vec4 yzww(const Vec4& v) { return Vec4(v.y, v.z, v.w, v.w); }

inline const char* OpName(TfxBytecode);


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
	std::unordered_map<uint32_t, float_t> channel_floats,
    void*  /*user_samp_hook*/ = nullptr,
    bool trace = true)
{
    using namespace tfx_eval_detail;

    std::vector<Vec4> stack; stack.reserve(64);
    auto push = [&](const Vec4& v) { stack.push_back(v); };
    auto pop1 = [&](size_t ip, const char* opname) -> Vec4 {
        if (stack.empty()) {
            if (trace) std::fprintf(stderr, "[eval] stack underflow before %s at ip=%zu, returning 0\n", opname, ip);
            return Vec4::zero();
        }
        auto v = stack.back(); stack.pop_back(); return v;
        };

    Mat4 cachedM{}; // for TransformVec4 following PushExternInputMat4

    for (size_t ip = 0; ip < ops.size(); ++ip) {
        const auto& i = ops[ip];
        if (trace) {
            const auto payload = DescribePayload(i);
            std::printf("ip=%04zu %-22s %s  (stack=%zu)\n",
                ip, OpName(i.op), payload.empty() ? "" : payload.c_str(), stack.size());
        }

        switch (i.op) {
            // ---------------- math ----------------
        case TfxBytecode::Add:
        case TfxBytecode::Add2: { auto b = pop1(ip, "Add"), a = pop1(ip, "Add"); push(a + b); break; }
        case TfxBytecode::Subtract: { auto b = pop1(ip, "Sub"), a = pop1(ip, "Sub"); push(a - b); break; }
        case TfxBytecode::Multiply:
        case TfxBytecode::Multiply2: { auto b = pop1(ip, "Mul"), a = pop1(ip, "Mul"); push(a * b); break; }
        case TfxBytecode::Divide: { auto b = pop1(ip, "Div"), a = pop1(ip, "Div"); push(a / b); break; }
        case TfxBytecode::Min: { auto b = pop1(ip, "Min"), a = pop1(ip, "Min"); push(min4(a, b)); break; }
        case TfxBytecode::Max: { auto b = pop1(ip, "Max"), a = pop1(ip, "Max"); push(max4(a, b)); break; }
        case TfxBytecode::IsZero: { auto a = pop1(ip, "IsZero"); push(is_zero_mask(a)); break; }
        case TfxBytecode::LessThan: { auto b = pop1(ip, "LT"), a = pop1(ip, "LT"); push(less_than_mask(a, b)); break; }
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

                                 // ------------- constants / externs -------------
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
            push(Vec4::splat(f)); break;
        }
        case TfxBytecode::PushExternInputVec4: {
            auto d = std::get<PushExternInputVec4Data>(i.data);
            // FIX: offset is in vec4s -> bytes = offset * 16
            push(externs.getVec4(d.ext, size_t(d.offset) * 16));
            break;
        }

        case TfxBytecode::PushExternInputMat4: {
            auto d = std::get<PushExternInputMat4Data>(i.data);
            Mat4 m = externs.getMat4(d.ext, size_t(d.offset) * 16);

            // cache for TransformVec4 and expose rows in temps (some bytecode reads them)
            cachedM = m;
            temp[0] = m.x_axis;
            temp[1] = m.y_axis;
            temp[2] = m.z_axis;
            temp[3] = m.w_axis;

            // If the next instruction is PopOutputMat4, push rows now so it can pop them.
            // PopOutputMat4 pops in order r3,r2,r1,r0, so we must push r0,r1,r2,r3 (LIFO).
            if (ip + 1 < ops.size() && ops[ip + 1].op == TfxBytecode::PopOutputMat4) {
                push(m.x_axis);
                push(m.y_axis);
                push(m.z_axis);
                push(m.w_axis);
            }
            // Otherwise, do NOT push anything—TransformVec4 will use cachedM.
            break;
        }

        case TfxBytecode::TransformVec4: {
            auto v = pop1(ip, "Xform vec");
            push(mul_vec4_mat(v, cachedM));
            break;
        }

                                       // ----------- channels (NEW: actually fetch) -----------
        case TfxBytecode::PushGlobalChannelVector: {
            const auto d = std::get<PushGlobalChannelVectorData>(i.data);
            // index is in vec4 units ? bytes = index * 16
            Vec4 v = externs.getVec4(TfxExtern::Generic, size_t(d.unk1) * 16);
            push(v);
            break;
        }
        case TfxBytecode::PushObjectChannelVector: {
            const auto* d = std::get_if<PushObjectChannelVectorData>(&i.data);
			
            if (!channel_floats.empty()) {
                //printf("PushObjectChannelVector: hash_be=0x%08X\n", d->hash_be);
                const auto it = channel_floats.find(d->hash_be);
                const float_t float_value = (it != channel_floats.end()) ? it->second : 1.0f;
                push(Vec4::splat(float_value));
            }
            else {
                                // Fallback: push 1.0f if no channel floats provided
				push(Vec4::splat(1.0f));
            }

            // Intentionally do nothing else (no push to the VM stack, etc.)
            break;
        }
        case TfxBytecode::PushTexDimensions: {; 
           
            push(Vec4(64.0f,64.0f,1.0f,1.0f));
            break;
        }

                                                 // ------------- temps / outputs -------------
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
            if (trace) std::printf("    -> cb[%u] = %s\n", d.element, to_str(v).c_str());
            cb[d.element] = v; break;
        }
        case TfxBytecode::Spline8Const: {
            auto d = std::get<SplineConstData>(i.data);
            // x comes from stack top
            Vec4 X = pop1(ip, "Spline8Const");

            // Load 10 constants starting at constant_start (C:0..3, D:4..7, thresholds:8,9)
            auto load = [&](size_t rel) -> Vec4 {
                size_t k = size_t(d.constant_start) + rel;
                return (k < constants.size()) ? constants[k] : Vec4::zero();
                };
            Vec4 C3 = load(0), C2 = load(1), C1 = load(2), C0 = load(3);
            Vec4 D3 = load(4), D2 = load(5), D1 = load(6), D0 = load(7);
            Vec4 Cth = load(8), Dth = load(9);

            // Estrin cubic on each bank
            Vec4 X2 = X * X;
            Vec4 Chigh = C3 * X + C2;
            Vec4 Clow = C1 * X + C0;
            Vec4 Ceval = Chigh * X2 + Clow;

            Vec4 Dhigh = D3 * X + D2;
            Vec4 Dlow = D1 * X + D0;
            Vec4 Deval = Dhigh * X2 + Dlow;

            // Threshold masks, channel XOR masks
            Vec4 Cmask = step4(Cth, X);
            Vec4 Dmask = step4(Dth, X);

            Vec4 Cchan = Vec4(xor01(Cmask, yzww(Cmask)).x,
                xor01(Cmask, yzww(Cmask)).y,
                xor01(Cmask, yzww(Cmask)).z,
                Cmask.w); // keep .w
            Vec4 Dchan = Vec4(xor01(Dmask, yzww(Dmask)).x,
                xor01(Dmask, yzww(Dmask)).y,
                xor01(Dmask, yzww(Dmask)).z,
                Dmask.w);

            // Masked contributions + horizontal sum
            float Csum = hsum4(Ceval * Cchan);
            float Dsum = hsum4(Deval * Dchan);

            float spline = (Dmask.x > 0.f) ? Dsum : Csum;
            Vec4 result = Vec4::splat(spline);

            if (trace) {
                std::printf("    Spline8: Csum=%g Dsum=%g use=%s\n",
                    Csum, Dsum, (Dmask.x > 0.f ? "D" : "C"));
            }

            push(result);
            break;
        }
        case TfxBytecode::PopOutputMat4: {
            auto d = std::get<PopOutputMat4Data>(i.data);
            if (d.element + 3 >= cb.size()) cb.resize(d.element + 4, Vec4::zero());
            Vec4 r3 = pop1(ip, "PopMat"), r2 = pop1(ip, "PopMat"), r1 = pop1(ip, "PopMat"), r0 = pop1(ip, "PopMat");
            if (trace) {
                std::printf("    -> cb[%u..%u] = %s, %s, %s, %s\n",
                    d.element, d.element + 3, to_str(r0).c_str(), to_str(r1).c_str(), to_str(r2).c_str(), to_str(r3).c_str());
            }
            cb[d.element + 0] = r0; cb[d.element + 1] = r1; cb[d.element + 2] = r2; cb[d.element + 3] = r3;
            break;
        }

                                       // ------------- resource/sampler ops: no buffer effect -------------
        case TfxBytecode::PushSampler:
        case TfxBytecode::SetShaderSampler:
        case TfxBytecode::SetShaderTexture:
        case TfxBytecode::SetShaderUav:
        case TfxBytecode::PushExternInputTextureView:
        case TfxBytecode::PushExternInputU32:
        case TfxBytecode::PushExternInputUav:
        case TfxBytecode::Unk49:
        case TfxBytecode::Unk4c:
        case TfxBytecode::Unk50:
        case TfxBytecode::Unk51:
        case TfxBytecode::PushTexTileParams:
        case TfxBytecode::PushTexTileCount:
        case TfxBytecode::Unk42:
        case TfxBytecode::Unk55:
        case TfxBytecode::Unk56:
        case TfxBytecode::Unk57:
        case TfxBytecode::Unk58:
            // Ignored here on purpose; binding is handled elsewhere.
            break;

        default:
            if (trace) std::fprintf(stderr, "[eval] unhandled opcode %s at ip=%zu\n", OpName(i.op), ip);
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

        // Try to read the payload; adjust field name if needed.
        if (const auto* d = std::get_if<PushObjectChannelVectorData>(&ins.data)) {
            // ---- Pick the correct field name for your payload struct ----
            // Common patterns in codebases: d->index, d->channel, d->unk1
			const uint32_t key = d->hash_be;

            out.push_back(key);
        }
        // If the instruction encodes the id in a unified type used by both Global/Object,
        // add another branch here (e.g., std::get_if<ChannelVectorData>(...)).
    }

    return out;
}