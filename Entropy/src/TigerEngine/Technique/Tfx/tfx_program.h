#pragma once
#include "tfx_eval.h"
#include "tfx_decompiler.h"

class TfxProgram {
public:
    std::vector<TfxData> ops;
    std::vector<Vec4>    constants;

    static TfxProgram FromBytecode(const std::vector<uint8_t>& bc,
        const std::vector<Vec4>& consts) {
        TfxProgram p;
        p.ops = ParseAll(bc);
        p.constants = consts;
        return p;
    }

    // Evaluate into cb0 (vector<float4>)
    void Evaluate(const ExternStorage& externs, std::vector<Vec4>& cb) const {
        std::array<Vec4, 16> temp{};
        EvaluateExpressionEoF(ops, externs, cb, constants, temp, nullptr, nullptr, false);
    }
    void Evaluate_Trace(const ExternStorage& externs, std::vector<Vec4>& cb) const {
        std::array<Vec4, 16> temp{};
        EvaluateExpressionEoF(ops, externs, cb, constants, temp, nullptr, nullptr);
    }

    std::string DecompilePretty() const {
        auto r = TfxBytecodeDecompiler::decompile(ops, constants);
        return r.pretty_print();
    }
};
