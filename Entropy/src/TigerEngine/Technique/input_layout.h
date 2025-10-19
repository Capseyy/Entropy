#pragma once
#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>
#include <format>
#include <d3dcompiler.h>
#include "Renderer/Tools/ErrorLogger.h"

struct TigerInputLayoutElement {
    std::string hlsl_type;
    DXGI_FORMAT DxgiFormat;
    uint32_t    _stride;
    std::string semantic_name;
    uint32_t    semantic_index;
    uint32_t    buffer_index;
    bool        is_instance_data;
};

struct TigerInputLayout {
    std::vector<TigerInputLayoutElement> elements;
};

// Declaration only – definition lives in a single .cpp
extern const std::array<TigerInputLayout, 77> INPUT_LAYOUTS;

// Factory – implemented once in a .cpp
HRESULT CreateInputLayoutFromTigerLayout(ID3D11Device* device, const TigerInputLayout& layout, Microsoft::WRL::ComPtr<ID3D11InputLayout>& outLayout);
