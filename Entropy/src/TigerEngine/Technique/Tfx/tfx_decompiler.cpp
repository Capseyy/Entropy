#include "tfx_decompiler.h"
#include <cassert>
#include <cstdio>

static void push(std::vector<std::string>& st, std::string s) { st.push_back(std::move(s)); }
static std::string pop(std::vector<std::string>& st) { auto s = std::move(st.back()); st.pop_back(); return s; }

// ---- extern name + formatter helpers (for pretty output) --------------------
static inline const char* ExtName(TfxExtern e) {
    switch (e) {
    case TfxExtern::Frame:          return "frame";
    case TfxExtern::View:           return "view";
    case TfxExtern::Deferred:       return "deferred";
    case TfxExtern::DeferredLight:  return "deferred_light";
    case TfxExtern::DeferredShadow: return "deferred_shadow";
    case TfxExtern::Transparent:    return "transparent";
    case TfxExtern::RigidModel:     return "rigid_model";
    case TfxExtern::Decal:          return "decal";
    case TfxExtern::SimpleGeometry: return "simple_geometry";
    case TfxExtern::Atmosphere:     return "atmosphere";
    case TfxExtern::Water:          return "water";
    case TfxExtern::Hdao:           return "hdao";
    case TfxExtern::GlobalLighting: return "global_lighting";
    case TfxExtern::Cubemaps:       return "cubemaps";
    case TfxExtern::SpeedtreePlacements: return "speedtree_placements";
    case TfxExtern::DecoratorWind:  return "decorator_wind";
    case TfxExtern::Postprocess:    return "postprocess";
    case TfxExtern::ShadowMask:     return "shadowmask";
    case TfxExtern::Fxaa:           return "fxaa";
    default:                        return "extern";
    }
}

static inline std::string extern_expr(const char* ty, TfxExtern ext, size_t byteOffset) {
    char buf[96];
    std::snprintf(buf, sizeof(buf), "%s+0x%zX", ExtName(ext), byteOffset);
    return std::string("extern<") + ty + ">(" + buf + ")";
}


std::string DecompilationResult::pretty_print() const {
    std::string r;
    if (!samplers.empty()) {
        r += "// Samplers\n";
        for (auto& s : samplers) {
            size_t slot; TfxShaderStage st; std::string expr;
            std::tie(slot, expr) = s;
            char buf[256]; std::snprintf(buf, sizeof(buf), "SamplerState s%zu = %s;\n", slot, expr.c_str());
            r += buf;
        }
        r += "\n";
    }
    if (!textures.empty()) {
        r += "// Textures\n";
        for (auto& t : textures) {
            size_t slot; TfxShaderStage st; std::string expr;
            std::tie(slot, expr) = t;
            char buf[256]; std::snprintf(buf, sizeof(buf), "Texture<float4> t%zu = %s;\n", slot, expr.c_str());
            r += buf;
        }
        r += "\n";
    }
    if (!uavs.empty()) {
        r += "// UAVs\n";
        for (auto& u : uavs) {
            size_t slot; TfxShaderStage st; std::string expr;
            std::tie(slot, expr) = u;
            char buf[256]; std::snprintf(buf, sizeof(buf), "RWTexture<float4> u%zu = %s;\n", slot, expr.c_str());
            r += buf;
        }
        r += "\n";
    }
    if (!cb_expressions.empty()) {
        r += "// Constant buffer\n";
        for (auto& kv : cb_expressions) {
            char buf[1024];
            std::snprintf(buf, sizeof(buf), "cb0[%zu] = %s;\n", kv.first, kv.second.c_str());
            r += buf;
        }
    }
    return r;
}

DecompilationResult TfxBytecodeDecompiler::decompile(
    const std::vector<TfxData>& ops,
    const std::vector<Vec4>& constants)
{
    DecompilationResult out;

    // ----- expression stack: STRINGS, not Vec4 -----
    std::vector<std::string> st; st.reserve(64);

    auto pushS = [&](std::string v) {
        st.emplace_back(std::move(v));
        };
    auto popS = [&]() -> std::string {
        assert(!st.empty());
        std::string v = std::move(st.back());
        st.pop_back();
        return v;
        };

    // float4 printer for constants
    auto f4 = [](const Vec4& v) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "float4(%g, %g, %g, %g)", v.x, v.y, v.z, v.w);
        return std::string(buf);
        };

    // decode 2-bit-per-component swizzle (b7..b6 -> x, b5..b4 -> y, b3..b2 -> z, b1..b0 -> w)
    auto swizzle4 = [](const std::string& a, uint8_t fields) {
        static const char comp[4] = { 'x','y','z','w' };
        char mask[6];
        mask[0] = '.';
        mask[1] = comp[(fields >> 6) & 3];
        mask[2] = comp[(fields >> 4) & 3];
        mask[3] = comp[(fields >> 2) & 3];
        mask[4] = comp[(fields >> 0) & 3];
        mask[5] = '\0';
        return "(" + a + ")" + std::string(mask);
        };

    for (const auto& i : ops)
    {
        switch (i.op)
        {
            // ---------- math ----------
        case TfxBytecode::Add:
        case TfxBytecode::Add2: {
            auto b = popS(), a = popS();
            pushS("(" + a + " + " + b + ")");
            break;
        }
        case TfxBytecode::Subtract: {
            auto b = popS(), a = popS();
            pushS("(" + a + " - " + b + ")");
            break;
        }
        case TfxBytecode::Multiply:
        case TfxBytecode::Multiply2: {
            auto b = popS(), a = popS();
            pushS("(" + a + " * " + b + ")");
            break;
        }
        case TfxBytecode::Divide: {
            auto b = popS(), a = popS();
            pushS("(" + a + " / " + b + ")");
            break;
        }
        case TfxBytecode::Frac: {
            auto a = popS(); pushS("frac(" + a + ")"); break;
        }
        case TfxBytecode::Saturate: {
            auto a = popS(); pushS("saturate(" + a + ")"); break;
        }
        case TfxBytecode::Merge_1_3: {
            auto b = popS(), a = popS();
            pushS("float4((" + a + ").x, (" + b + ").xyz)");
            break;
        }
        case TfxBytecode::Merge_2_2: {
            auto b = popS(), a = popS();
            pushS("float4((" + a + ").xy, (" + b + ").xy)");
            break;
        }
        case TfxBytecode::Merge_3_1: {
            auto b = popS(), a = popS();
            pushS("float4((" + a + ").xyz, (" + b + ").x)");
            break;
        }

                                   // ---------- constants ----------
        case TfxBytecode::PushConstantVec4: {
            const auto d = std::get<PushConstantVec4Data>(i.data);
            const Vec4 v = (d.constant_index < constants.size()) ? constants[d.constant_index] : Vec4{ 0,0,0,0 };
            pushS(f4(v));
            break;
        }
        case TfxBytecode::LerpConstant: {
            const auto d = std::get<LerpConstantData>(i.data);
            auto t = popS();
            const Vec4 a = (d.constant_start < constants.size()) ? constants[d.constant_start] : Vec4{ 0,0,0,0 };
            const Vec4 b = (d.constant_start + 1 < constants.size()) ? constants[d.constant_start + 1] : Vec4{ 0,0,0,0 };
            pushS("lerp(" + f4(a) + ", " + f4(b) + ", " + t + ")");
            break;
        }
        case TfxBytecode::LerpConstantSaturated: {
            const auto d = std::get<LerpConstantData>(i.data);
            auto t = popS();
            const Vec4 a = (d.constant_start < constants.size()) ? constants[d.constant_start] : Vec4{ 0,0,0,0 };
            const Vec4 b = (d.constant_start + 1 < constants.size()) ? constants[d.constant_start + 1] : Vec4{ 0,0,0,0 };
            pushS("saturate(lerp(" + f4(a) + ", " + f4(b) + ", " + t + "))");
            break;
        }

                                               // ---------- externs (optional; keep commented if you don’t have a path helper) ----------
                                               //case TfxBytecode::PushExternInputFloat: {
                                               //     const auto d = std::get<PushExternInputFloatData>(i.data);
                                               //     const size_t off = size_t(d.offset) * 4;
                                               //     pushS("extern<float>(" + ExternStorage::get_field_path(d.ext, off) + ")");
                                               //     break;
                                               // }

                                               // ---------- resources ----------
        case TfxBytecode::PushSampler: {
            const auto d = std::get<PushSamplerData>(i.data);
            pushS("get_sampler(" + std::to_string(d.index) + ")");
            break;
        }
        case TfxBytecode::SetShaderSampler: {
            const auto d = std::get<SetShaderBindingData>(i.data);
            auto v = popS();
            // if your DecompilationResult stores stage, add d.stage too
            out.samplers.emplace_back(d.slot, v);
            break;
        }
        case TfxBytecode::SetShaderTexture: {
            const auto d = std::get<SetShaderBindingData>(i.data);
            auto v = popS();
            out.textures.emplace_back(d.slot, v);
            break;
        }
        case TfxBytecode::SetShaderUav: {
            const auto d = std::get<SetShaderBindingData>(i.data);
            auto v = popS();
            out.uavs.emplace_back(d.slot, v);
            break;
        }

                                      // ---------- cb and temps ----------
        case TfxBytecode::PushFromOutput: {
            const auto d = std::get<PushFromOutputData>(i.data);
            pushS("cb0[" + std::to_string(d.element) + "]");
            break;
        }
        case TfxBytecode::PopOutput: {
            const auto d = std::get<PopOutputData>(i.data);
            auto v = popS();
            out.cb_expressions.emplace_back(d.element, v);
            break;
        }

                                   // ---------- swizzles ----------
        case TfxBytecode::PermuteAllX: {
            auto a = popS();
            pushS("float4((" + a + ").x, (" + a + ").x, (" + a + ").x, (" + a + ").x)");
            break;
        }
        case TfxBytecode::Permute: {
            const auto d = std::get<PermuteData>(i.data);
            auto a = popS();
            pushS(swizzle4(a, d.fields));
            break;
        }

        case TfxBytecode::PushExternInputTextureView: {
            // nice to see where textures come from in the pretty output:
            const auto d = std::get<PushExternInputTexData>(i.data);
            const size_t off = size_t(d.offset) * 8;   // texture/u64 handles are 8 bytes in the spec
            pushS(extern_expr("Texture", d.ext, off));
            break;
        }
        case TfxBytecode::PushExternInputMat4: {
            const auto d = std::get<PushExternInputMat4Data>(i.data);
            const size_t off = size_t(d.offset) * 16;  // offset is in vec4s for mat4 too
            // If your pipeline later pops 4 rows into cb, this string is still useful context.
            pushS(extern_expr("float4x4", d.ext, off));
            break;
        }
        case TfxBytecode::PushExternInputFloat: {
            const auto d = std::get<PushExternInputFloatData>(i.data);
            const size_t off = size_t(d.offset) * 4;   // floats are 4 bytes
            pushS(extern_expr("float", d.ext, off));
            break;
        }
        case TfxBytecode::PushExternInputVec4: {
            const auto d = std::get<PushExternInputVec4Data>(i.data);
            const size_t off = size_t(d.offset) * 16;  // vec4s are 16 bytes
            pushS(extern_expr("float4", d.ext, off));
            break;
        }

        default:
            // Fallback textual marker so output is still useful
            pushS("/*op:" + std::to_string(int(i.op)) + "*/");
            break;
        }
    }

    return out;
}