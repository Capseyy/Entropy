#pragma once
#include <string>
#include <vector>
#include "tfx.h"

struct DecompilationResult {
    std::vector<std::tuple<size_t, std::string>> textures;
    std::vector<std::tuple<size_t, std::string>> samplers;
    std::vector<std::tuple<size_t, std::string>> uavs;
    std::vector<std::pair<size_t, std::string>> cb_expressions;

    std::string pretty_print() const;
};

struct TfxBytecodeDecompiler {
    static DecompilationResult decompile(const std::vector<TfxData>& ops,
        const std::vector<Vec4>& constants);
};
