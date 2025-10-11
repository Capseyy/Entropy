#include <string>
#include <vector>
#include <array>
#include <cstdint>
#include <d3d11.h>
#include <wrl/client.h>
#include "Renderer/Tools/ErrorLogger.h"
#include <d3dcompiler.h>
#include <format>


struct TigerInputLayoutElement {
    std::string hlsl_type;
    DXGI_FORMAT DxgiFormat;
    uint32_t _stride;
    std::string semantic_name;
    uint32_t semantic_index;
    uint32_t buffer_index;
    bool is_instance_data;
};

struct TigerInputLayout {
    std::vector<TigerInputLayoutElement> elements;
};

// Now define the input layouts
const std::array<TigerInputLayout, 77> INPUT_LAYOUTS = {
    // Layout 0
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false }
        }
    },
    // Layout 1
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false }
        }
    },
    // Layout 2
    TigerInputLayout{
        {
            { "float2", DXGI_FORMAT_R32G32_FLOAT,      8, "POSITION", 0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT,      8, "TEXCOORD", 0, 0, false },
            { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,    4, "COLOR",    0, 0, false }
        }
    },
    // Layout 3
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT,    8, "TEXCOORD", 0, 0, false },
            { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,  4, "COLOR",    0, 0, false }
        }
    },
    // Layout 4
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
            { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,  4, "COLOR",     0, 0, false }
        }
    },
    // Layout 5
    TigerInputLayout{
        {
            { "float2", DXGI_FORMAT_R32G32_FLOAT, 8, "POSITION", 0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT, 8, "TEXCOORD", 0, 0, false }
        }
    },
    // Layout 6
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "POSITION", 0, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "NORMAL",   0, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TANGENT",  0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "TEXCOORD", 0, 0, false }
        }
    },
    // Layout 7
    TigerInputLayout{
        {
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "NORMAL",   0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
            { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 1, false }
        }
    },
    // Layout 8
    TigerInputLayout{
        {
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
            { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 1, false }
        }
    },
    // Layout 9
    TigerInputLayout{
        {
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
            { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "NORMAL",   0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
            { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "COLOR",    0, 0, false }
        }
    },
    // Layout 10
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "POSITION", 0, 0, false },
            { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "NORMAL",   0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "TEXCOORD", 1, 0, false }
        }
    },
    // Layout 11
    TigerInputLayout{
        {
            { "float2", DXGI_FORMAT_R32G32_FLOAT,     8, "POSITION", 0, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "TEXCOORD", 0, 0, false }
        }
    },
    // Layout 12
    TigerInputLayout{
        {
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
            { "float2", DXGI_FORMAT_R32G32_FLOAT,     8, "TEXCOORD", 0, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "NORMAL",   0, 0, false }
        }
    },
    // Layout 13
    TigerInputLayout{
        {
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "POSITION",     0, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "NORMAL",       0, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TANGENT",      0, 0, false },
            { "float2", DXGI_FORMAT_R16G16_SNORM,        4, "TEXCOORD",     0, 1, false },
            { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "BLENDWEIGHT",  0, 2, false },
            { "uint4",  DXGI_FORMAT_R8G8B8A8_UINT,       4, "BLENDINDICES", 0, 2, false }
        }
    },
    // Layout 14
    TigerInputLayout{
        {
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "POSITION",     0, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "NORMAL",       0, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TANGENT",      0, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     0, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     1, 0, false },
            { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     2, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD",     3, 0, false },
            { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD",     4, 0, false },
        }
    },
};

HRESULT CreateInputLayoutFromTigerLayout(ID3D11Device* device, const TigerInputLayout& layout, Microsoft::WRL::ComPtr<ID3D11InputLayout>& outLayout, D3D11_INPUT_ELEMENT_DESC& outDesc);