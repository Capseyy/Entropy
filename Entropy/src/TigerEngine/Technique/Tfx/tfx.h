#pragma once
#include <cstdint>
#include <vector>
#include <variant>
#include <stdexcept>
#include <cstdio>
#include <unordered_map>           // <-- added
#include "tfx_runtime.h"

enum class TfxExtern : uint8_t {
    None = 0,
    Frame = 1,   // <- game_time, render_time, etc.
    World = 2,
    View = 3,
    // ...
};

// ====================== OPCODES (EoF exact) ======================
enum class TfxBytecode : uint8_t {
    Add = 0x01, Subtract = 0x02, Multiply = 0x03, Divide = 0x04,
    Multiply2 = 0x05, Add2 = 0x06, IsZero = 0x07, Min = 0x08, Max = 0x09,
    LessThan = 0x0A, Dot = 0x0B, Merge_1_3 = 0x0C, Merge_2_2 = 0x0D, Merge_3_1 = 0x0E,
    Cubic = 0x0F, Lerp = 0x10, LerpSaturated = 0x11, MultiplyAdd = 0x12, Clamp = 0x13, Unk14 = 0x14,
    Abs = 0x15, Sign = 0x16, Floor = 0x17, Ceil = 0x18, Round = 0x19, Frac = 0x1A,
    Unk1b = 0x1B, Unk1c = 0x1C, Negate = 0x1D,
    VecRotSin = 0x1E, VecRotCos = 0x1F, VecRotSinCos = 0x20,
    PermuteAllX = 0x21, Permute = 0x22, Saturate = 0x23,
    Unk25 = 0x25, Unk26 = 0x26, Triangle = 0x27, Jitter = 0x28, Wander = 0x29, Rand = 0x2A, RandSmooth = 0x2B,
    Unk2c = 0x2C, Unk2d = 0x2D, TransformVec4 = 0x2E,

    Unk34_EoF = 0x34, Unk35_EoF = 0x35, Unk36_EoF = 0x36, Unk37_EoF = 0x37,
    Unk38_EoF = 0x38, Unk39_EoF = 0x39, Unk3A_EoF = 0x3A,

    PushConstantVec4 = 0x3B, LerpConstant = 0x3C, LerpConstantSaturated = 0x3D,
    Spline4Const = 0x3E, Spline8Const = 0x3F, Spline8ConstChain = 0x40,
    Gradient4Const = 0x41, Gradient8Const = 0x42,

    PushExternInputFloat = 0x43, PushExternInputVec4 = 0x44, PushExternInputMat4 = 0x45,
    PushExternInputTextureView = 0x46, PushExternInputU32 = 0x47, PushExternInputUav = 0x48,

    Unk42 = 0x49,
    PushFromOutput = 0x4A, PopOutput = 0x4B, PopOutputMat4 = 0x4C,
    PushTemp = 0x4D, PopTemp = 0x4E,

    SetShaderTexture = 0x4F, Unk49 = 0x50, SetShaderSampler = 0x51, SetShaderUav = 0x52,
    Unk4c = 0x53, PushSampler = 0x54,
    PushObjectChannelVector = 0x55, PushGlobalChannelVector = 0x56,
    Unk50 = 0x57, Unk51 = 0x58,

    PushTexDimensions = 0x59, PushTexTileParams = 0x5A, PushTexTileCount = 0x5B,
    Unk55 = 0x5C, Unk56 = 0x5D, Unk57 = 0x5E, Unk58 = 0x5F,

    Unknown = 0xFF
};

// ====================== EXTERNS ======================

struct FrameExtern {
    float game_time;       // offset 0 * 4 bytes
    float render_time;     // offset 1
    float delta_game_time; // offset 2
    float exposure_time;   // offset 3
    
};


struct ExternStorage {
    struct Blob { const uint8_t* ptr{ nullptr }; size_t size{ 0 }; };
    std::unordered_map<TfxExtern, Blob> blobs;

    template <class T>
    void set(TfxExtern id, const T& pod) {
        blobs[id] = { reinterpret_cast<const uint8_t*>(&pod), sizeof(T) };
    }
    void set_raw(TfxExtern id, const void* p, size_t n) { blobs[id] = { (const uint8_t*)p, n }; }

    float getFloat(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end() || byteOffset + sizeof(float) > it->second.size) return 0.0f;
        float v; std::memcpy(&v, it->second.ptr + byteOffset, sizeof(v));
        return v;
    }
    Vec4 getVec4(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end() || byteOffset + sizeof(Vec4) > it->second.size) return Vec4::zero();
        Vec4 v; std::memcpy(&v, it->second.ptr + byteOffset, sizeof(v));
        return v;
    }
    Mat4 getMat4(TfxExtern id, size_t byteOffset) const {
        auto it = blobs.find(id);
        if (it == blobs.end() || byteOffset + sizeof(Mat4) > it->second.size) return Mat4{};
        Mat4 m; std::memcpy(&m, it->second.ptr + byteOffset, sizeof(m));
        return m;
    }
};
// ====================== PAYLOADS ======================
struct PermuteData { uint8_t fields; };
struct PushConstantVec4Data { uint8_t constant_index; };
struct LerpConstantData { uint8_t constant_start; };
struct SplineConstData { uint8_t constant_start; };
struct GradientConstData { uint8_t constant_start; };

struct PushFromOutputData { uint8_t element; };
struct PopOutputData { uint8_t element; };
struct PopOutputMat4Data { uint8_t element; };

struct PushExternInputFloatData { TfxExtern ext; uint8_t offset; };
struct PushExternInputVec4Data { TfxExtern ext; uint8_t offset; };
struct PushExternInputMat4Data { TfxExtern ext; uint8_t offset; };
struct PushExternInputTexData { TfxExtern ext; uint8_t offset; };
struct PushExternInputU32Data { TfxExtern ext; uint8_t offset; };
struct PushExternInputUavData { TfxExtern ext; uint8_t offset; };

struct PushTempData { uint8_t slot; };
struct PopTempData { uint8_t slot; };
struct PushSamplerData { uint8_t index; };

struct PushObjectChannelVectorData { uint32_t hash_be; };
struct PushGlobalChannelVectorData { uint8_t unk1; };

struct PushTexParamData { uint8_t index; uint8_t fields; };
struct OneU8 { uint8_t v; };

// Packed ttttsssss binder byte
struct SetShaderBindingData { uint8_t value; uint8_t stage; uint8_t slot; };
inline SetShaderBindingData DecodeBinding(uint8_t v) {
    SetShaderBindingData d{};
    d.value = v; d.stage = (v >> 5) & 0x7; d.slot = v & 0x1F;
    return d;
}

using TfxPayload = std::variant<std::monostate,
    PermuteData,
    PushConstantVec4Data, LerpConstantData,
    SplineConstData, GradientConstData,
    PushFromOutputData, PopOutputData, PopOutputMat4Data,
    PushExternInputFloatData, PushExternInputVec4Data, PushExternInputMat4Data,
    PushExternInputTexData, PushExternInputU32Data, PushExternInputUavData,
    PushTempData, PopTempData, PushSamplerData,
    SetShaderBindingData, OneU8,
    PushTexParamData,
    PushObjectChannelVectorData, PushGlobalChannelVectorData>;

struct TfxData { TfxBytecode op{ TfxBytecode::Unknown }; TfxPayload data{}; };

// ====================== READER UTILS ======================
struct ByteReader {
    const std::vector<uint8_t>& buf; size_t pos{ 0 };
    size_t remaining() const { return (pos <= buf.size()) ? (buf.size() - pos) : 0; }
    bool eof() const { return remaining() == 0; }
    uint8_t u8() { if (eof()) throw std::runtime_error("EOF"); return buf[pos++]; }
    uint32_t u32be() { return (uint32_t(u8()) << 24) | (uint32_t(u8()) << 16) | (uint32_t(u8()) << 8) | uint32_t(u8()); }
};

// ====================== NAMES ======================
inline const char* OpName(TfxBytecode op) {
    switch (op) {
    case TfxBytecode::Add: return "Add";
    case TfxBytecode::Subtract: return "Subtract";
    case TfxBytecode::Multiply: return "Multiply";
    case TfxBytecode::Divide: return "Divide";
    case TfxBytecode::Multiply2: return "Multiply2";
    case TfxBytecode::Add2: return "Add2";
    case TfxBytecode::IsZero: return "IsZero";
    case TfxBytecode::Min: return "Min";
    case TfxBytecode::Max: return "Max";
    case TfxBytecode::LessThan: return "LessThan";
    case TfxBytecode::Dot: return "Dot";
    case TfxBytecode::Merge_1_3: return "Merge_1_3";
    case TfxBytecode::Merge_2_2: return "Merge_2_2";
    case TfxBytecode::Merge_3_1: return "Merge_3_1";
    case TfxBytecode::Cubic: return "Cubic";
    case TfxBytecode::Lerp: return "Lerp";
    case TfxBytecode::LerpSaturated: return "LerpSaturated";
    case TfxBytecode::MultiplyAdd: return "MultiplyAdd";
    case TfxBytecode::Clamp: return "Clamp";
    case TfxBytecode::Unk14: return "Unk14";
    case TfxBytecode::Abs: return "Abs";
    case TfxBytecode::Sign: return "Sign";
    case TfxBytecode::Floor: return "Floor";
    case TfxBytecode::Ceil: return "Ceil";
    case TfxBytecode::Round: return "Round";
    case TfxBytecode::Frac: return "Frac";
    case TfxBytecode::Unk1b: return "Unk1b";
    case TfxBytecode::Unk1c: return "Unk1c";
    case TfxBytecode::Negate: return "Negate";
    case TfxBytecode::VecRotSin: return "VecRotSin";
    case TfxBytecode::VecRotCos: return "VecRotCos";
    case TfxBytecode::VecRotSinCos: return "VecRotSinCos";
    case TfxBytecode::PermuteAllX: return "PermuteAllX";
    case TfxBytecode::Permute: return "Permute";
    case TfxBytecode::Saturate: return "Saturate";
    case TfxBytecode::Unk25: return "Unk25";
    case TfxBytecode::Unk26: return "Unk26";
    case TfxBytecode::Triangle: return "Triangle";
    case TfxBytecode::Jitter: return "Jitter";
    case TfxBytecode::Wander: return "Wander";
    case TfxBytecode::Rand: return "Rand";
    case TfxBytecode::RandSmooth: return "RandSmooth";
    case TfxBytecode::Unk2c: return "Unk2c";
    case TfxBytecode::Unk2d: return "Unk2d";
    case TfxBytecode::TransformVec4: return "TransformVec4";
    case TfxBytecode::PushConstantVec4: return "PushConstantVec4";
    case TfxBytecode::LerpConstant: return "LerpConstant";
    case TfxBytecode::LerpConstantSaturated: return "LerpConstantSaturated";
    case TfxBytecode::Spline4Const: return "Spline4Const";
    case TfxBytecode::Spline8Const: return "Spline8Const";
    case TfxBytecode::Spline8ConstChain: return "Spline8ConstChain";
    case TfxBytecode::Gradient4Const: return "Gradient4Const";
    case TfxBytecode::Gradient8Const: return "Gradient8Const";
    case TfxBytecode::PushExternInputFloat: return "PushExternInputFloat";
    case TfxBytecode::PushExternInputVec4: return "PushExternInputVec4";
    case TfxBytecode::PushExternInputMat4: return "PushExternInputMat4";
    case TfxBytecode::PushExternInputTextureView: return "PushExternInputTextureView";
    case TfxBytecode::PushExternInputU32: return "PushExternInputU32";
    case TfxBytecode::PushExternInputUav: return "PushExternInputUav";
    case TfxBytecode::Unk42: return "Unk42";
    case TfxBytecode::PushFromOutput: return "PushFromOutput";
    case TfxBytecode::PopOutput: return "PopOutput";
    case TfxBytecode::PopOutputMat4: return "PopOutputMat4";
    case TfxBytecode::PushTemp: return "PushTemp";
    case TfxBytecode::PopTemp: return "PopTemp";
    case TfxBytecode::SetShaderTexture: return "SetShaderTexture";
    case TfxBytecode::Unk49: return "Unk49";
    case TfxBytecode::SetShaderSampler: return "SetShaderSampler";
    case TfxBytecode::SetShaderUav: return "SetShaderUav";
    case TfxBytecode::Unk4c: return "Unk4c";
    case TfxBytecode::PushSampler: return "PushSampler";
    case TfxBytecode::PushObjectChannelVector: return "PushObjectChannelVector";
    case TfxBytecode::PushGlobalChannelVector: return "PushGlobalChannelVector";
    case TfxBytecode::Unk50: return "Unk50";
    case TfxBytecode::Unk51: return "Unk51";
    case TfxBytecode::PushTexDimensions: return "PushTexDimensions";
    case TfxBytecode::PushTexTileParams: return "PushTexTileParams";
    case TfxBytecode::PushTexTileCount: return "PushTexTileCount";
    case TfxBytecode::Unk55: return "Unk55";
    case TfxBytecode::Unk56: return "Unk56";
    case TfxBytecode::Unk57: return "Unk57";
    case TfxBytecode::Unk58: return "Unk58";
    default: return "Unknown";
    }
}

// ====================== PAYLOAD SIZE (for bounds check) ======================
inline size_t PayloadSizeFor(TfxBytecode op) {
    using B = TfxBytecode;
    switch (op) {
    case B::Permute: return 1;
    case B::PushConstantVec4: return 1;
    case B::LerpConstant: return 1;
    case B::LerpConstantSaturated: return 1;
    case B::Spline4Const: return 1;
    case B::Spline8Const: return 1;
    case B::Spline8ConstChain: return 1;
    case B::Gradient4Const: return 1;
    case B::Gradient8Const: return 1;

    case B::PushExternInputFloat: return 2;
    case B::PushExternInputVec4:  return 2;
    case B::PushExternInputMat4:  return 2;
    case B::PushExternInputTextureView: return 2;
    case B::PushExternInputU32:   return 2;
    case B::PushExternInputUav:   return 2;

    case B::PushFromOutput: return 1;
    case B::PopOutput: return 1;
    case B::PopOutputMat4: return 1;
    case B::PushTemp: return 1;
    case B::PopTemp: return 1;

    case B::SetShaderTexture: return 1;
    case B::SetShaderSampler: return 1;
    case B::SetShaderUav: return 1;
    case B::PushSampler: return 1;

    case B::PushObjectChannelVector: return 4;
    case B::PushGlobalChannelVector: return 1;

    case B::PushTexDimensions: return 2;
    case B::PushTexTileParams: return 2;
    case B::PushTexTileCount:  return 2;

    default: return 0;
    }
}

// ====================== PARSE ONE ======================
inline TfxData ReadTfxBytecodeOp(ByteReader& r) {
    TfxData out;
    out.op = static_cast<TfxBytecode>(r.u8());

    switch (out.op) {
    case TfxBytecode::Permute:                 out.data = PermuteData{ r.u8() }; break;

    case TfxBytecode::PushConstantVec4:        out.data = PushConstantVec4Data{ r.u8() }; break;
    case TfxBytecode::LerpConstant:            out.data = LerpConstantData{ r.u8() }; break;
    case TfxBytecode::LerpConstantSaturated:   out.data = LerpConstantData{ r.u8() }; break;
    case TfxBytecode::Spline4Const:            out.data = SplineConstData{ r.u8() }; break;
    case TfxBytecode::Spline8Const:            out.data = SplineConstData{ r.u8() }; break;
    case TfxBytecode::Spline8ConstChain:       out.data = SplineConstData{ r.u8() }; break;
    case TfxBytecode::Gradient4Const:          out.data = GradientConstData{ r.u8() }; break;
    case TfxBytecode::Gradient8Const:          out.data = GradientConstData{ r.u8() }; break;

    case TfxBytecode::PushExternInputFloat:    out.data = PushExternInputFloatData{ TfxExtern(r.u8()), r.u8() }; break;
    case TfxBytecode::PushExternInputVec4:     out.data = PushExternInputVec4Data{ TfxExtern(r.u8()), r.u8() }; break;
    case TfxBytecode::PushExternInputMat4:     out.data = PushExternInputMat4Data{ TfxExtern(r.u8()), r.u8() }; break;
    case TfxBytecode::PushExternInputTextureView: out.data = PushExternInputTexData{ TfxExtern(r.u8()), r.u8() }; break;
    case TfxBytecode::PushExternInputU32:      out.data = PushExternInputU32Data{ TfxExtern(r.u8()), r.u8() }; break;
    case TfxBytecode::PushExternInputUav:      out.data = PushExternInputUavData{ TfxExtern(r.u8()), r.u8() }; break;

    case TfxBytecode::PushFromOutput:          out.data = PushFromOutputData{ r.u8() }; break;
    case TfxBytecode::PopOutput:               out.data = PopOutputData{ r.u8() }; break;
    case TfxBytecode::PopOutputMat4:           out.data = PopOutputMat4Data{ r.u8() }; break;

    case TfxBytecode::PushTemp:                out.data = PushTempData{ r.u8() }; break;
    case TfxBytecode::PopTemp:                 out.data = PopTempData{ r.u8() }; break;

    case TfxBytecode::SetShaderTexture:
    case TfxBytecode::SetShaderSampler:
    case TfxBytecode::SetShaderUav:            out.data = DecodeBinding(r.u8()); break;

    case TfxBytecode::PushSampler:             out.data = PushSamplerData{ r.u8() }; break;

    case TfxBytecode::PushObjectChannelVector: out.data = PushObjectChannelVectorData{ r.u32be() }; break;
    case TfxBytecode::PushGlobalChannelVector: out.data = PushGlobalChannelVectorData{ r.u8() }; break;

    case TfxBytecode::PushTexDimensions:
    case TfxBytecode::PushTexTileParams:
    case TfxBytecode::PushTexTileCount:        out.data = PushTexParamData{ r.u8(), r.u8() }; break;

    default: /* no extra payload */ break;
    }
    return out;
}

// ====================== TRACE PRINTER ======================
inline void PrintOpTrace(size_t ip, uint8_t raw, const TfxData& d) {
    std::printf("ip=%04zu 0x%02X %-22s", ip, unsigned(raw), OpName(d.op));

    switch (d.op) {
    case TfxBytecode::Permute: {
        auto& p = std::get<PermuteData>(d.data);
        std::printf("  fields=0x%02X", p.fields);
        break;
    }
    case TfxBytecode::PushConstantVec4: {
        auto& p = std::get<PushConstantVec4Data>(d.data);
        std::printf("  const_index=%u", p.constant_index);
        break;
    }
    case TfxBytecode::LerpConstant:
    case TfxBytecode::LerpConstantSaturated: {
        auto& p = std::get<LerpConstantData>(d.data);
        std::printf("  const_start=%u%s", p.constant_start,
            d.op == TfxBytecode::LerpConstantSaturated ? " (sat)" : "");
        break;
    }
    case TfxBytecode::Spline4Const:
    case TfxBytecode::Spline8Const:
    case TfxBytecode::Spline8ConstChain: {
        auto& p = std::get<SplineConstData>(d.data);
        std::printf("  const_start=%u", p.constant_start);
        break;
    }
    case TfxBytecode::Gradient4Const:
    case TfxBytecode::Gradient8Const: {
        auto& p = std::get<GradientConstData>(d.data);
        std::printf("  const_start=%u", p.constant_start);
        break;
    }
    case TfxBytecode::PushExternInputFloat: {
        auto& p = std::get<PushExternInputFloatData>(d.data);
        std::printf("  extern=%u off=%u (bytes=%u)", unsigned(p.ext), p.offset, p.offset * 4u);
        break;
    }
    case TfxBytecode::PushExternInputVec4: {
        auto& p = std::get<PushExternInputVec4Data>(d.data);
        std::printf("  extern=%u off=%u (bytes=%u)", unsigned(p.ext), p.offset, p.offset * 16u);
        break;
    }
    case TfxBytecode::PushExternInputMat4: {
        auto& p = std::get<PushExternInputMat4Data>(d.data);
        std::printf("  extern=%u off=%u (bytes=%u)", unsigned(p.ext), p.offset, p.offset * 16u);
        break;
    }
    case TfxBytecode::PushFromOutput: {
        auto& p = std::get<PushFromOutputData>(d.data);
        std::printf("  element=%u", p.element);
        break;
    }
    case TfxBytecode::PopOutput: {
        auto& p = std::get<PopOutputData>(d.data);
        std::printf("  element=%u", p.element);
        break;
    }
    case TfxBytecode::PopOutputMat4: {
        auto& p = std::get<PopOutputMat4Data>(d.data);
        std::printf("  element=%u..%u", p.element, p.element + 3);
        break;
    }
    case TfxBytecode::PushTemp: {
        auto& p = std::get<PushTempData>(d.data);
        std::printf("  slot=%u", p.slot);
        break;
    }
    case TfxBytecode::PopTemp: {
        auto& p = std::get<PopTempData>(d.data);
        std::printf("  slot=%u", p.slot);
        break;
    }
    case TfxBytecode::PushSampler: {
        auto& p = std::get<PushSamplerData>(d.data);
        std::printf("  index=%u", p.index);
        break;
    }
    case TfxBytecode::SetShaderTexture:
    case TfxBytecode::SetShaderSampler:
    case TfxBytecode::SetShaderUav: {
        auto& b = std::get<SetShaderBindingData>(d.data);
        std::printf("  stage=%u slot=%u (raw=0x%02X)", b.stage, b.slot, b.value);
        break;
    }
    case TfxBytecode::PushObjectChannelVector: {
        auto& p = std::get<PushObjectChannelVectorData>(d.data);
        std::printf("  hash_be=0x%08X", p.hash_be);
        break;
    }
    case TfxBytecode::PushGlobalChannelVector: {
        auto& p = std::get<PushGlobalChannelVectorData>(d.data);
        std::printf("  unk1=%u", p.unk1);
        break;
    }
    case TfxBytecode::PushTexDimensions:
    case TfxBytecode::PushTexTileParams:
    case TfxBytecode::PushTexTileCount: {
        auto& p = std::get<PushTexParamData>(d.data);
        std::printf("  index=%u fields=0x%02X", p.index, p.fields);
        break;
    }
    default:
        // no payload or not decoded
        break;
    }
    std::printf("\n");
}

// ====================== PARSE ALL (with tracing) ======================
inline std::vector<TfxData> ParseAll(const std::vector<uint8_t>& bytecode, bool trace = false) {
    std::vector<TfxData> ops; ops.reserve(bytecode.size());
    ByteReader br{ bytecode, 0 };

    while (!br.eof()) {
        // Defensive size check before consuming
        uint8_t raw = br.buf[br.pos];
        TfxBytecode op = static_cast<TfxBytecode>(raw);
        size_t need = 1 + PayloadSizeFor(op);
        if (br.remaining() < need) {
            std::fprintf(stderr,
                "TFX parse truncated: need %zu bytes for op 0x%02X (%s), have %zu\n",
                need, unsigned(raw), OpName(op), br.remaining());
            break;
        }

        size_t ip = br.pos;
        TfxData d = ReadTfxBytecodeOp(br);
        if (trace) {
            PrintOpTrace(ip, raw, d);      // <-- detailed per-op print
        }
        ops.emplace_back(std::move(d));
    }
    return ops;
}
