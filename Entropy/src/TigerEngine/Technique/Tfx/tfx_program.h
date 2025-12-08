#pragma once
#include "tfx_eval.h"
#include "tfx_decompiler.h"
#include "Runtime/Assets/Technique.h"

class TfxProgram {
public:
    std::vector<TfxData> ops;
    std::vector<Vec4>    constants;
    std::vector<uint32_t> channels;
	uint32_t id = 0;
    static TfxProgram FromBytecode(const std::vector<uint8_t>& bc,
        const std::vector<Vec4>& consts, uint32_t id) {
        TfxProgram p;
        p.ops = ParseAll(bc);
        p.constants = consts;
		p.channels = CollectObjectChannelU32(p.ops);
		p.id = id;
        return p;
    }

    // Evaluate into cb0 (vector<float4>)
    void Evaluate(const ExternStorage& externs, std::vector<Vec4>& cb, std::vector<std::shared_ptr<EntropyAssets::Texture2DRes>> texs) const {
        std::array<Vec4, 16> temp{};
        EvaluateExpressionEoF(ops, externs, cb, constants, temp, {}, texs, id, false);
    }
    void Evaluate_Trace(const ExternStorage& externs, std::vector<Vec4>& cb, std::vector<std::shared_ptr<EntropyAssets::Texture2DRes>> texs) const {
        std::array<Vec4, 16> temp{};
        EvaluateExpressionEoF(ops, externs, cb, constants, temp, {}, texs, id);
    }
    void Evaluate_With_Channels(const ExternStorage& externs, std::vector<Vec4>& cb, std::unordered_map<uint32_t, float_t> channels, std::vector<std::shared_ptr<EntropyAssets::Texture2DRes>> texs, bool trace = false) const {
        std::array<Vec4, 16> temp{};
        EvaluateExpressionEoF(ops, externs, cb, constants, temp, channels,  texs, id,trace);
    }

    std::string DecompilePretty() const {
        auto r = TfxBytecodeDecompiler::decompile(ops, constants);
        return r.pretty_print();
    }
};
