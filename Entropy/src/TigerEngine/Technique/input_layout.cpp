#include "input_layout.h"


const std::array<TigerInputLayout, 77> INPUT_LAYOUTS = {
    // Layout 0
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
    }},
    // Layout 1
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
    }},
    // Layout 2
    TigerInputLayout{{
        { "float2", DXGI_FORMAT_R32G32_FLOAT,    8, "POSITION", 0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,    8, "TEXCOORD", 0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,  4, "COLOR",    0, 0, false },
    }},
    // Layout 3
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,    8, "TEXCOORD", 0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,  4, "COLOR",    0, 0, false },
    }},
    // Layout 4
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,  4, "COLOR",    0, 0, false },
    }},
    // Layout 5
    TigerInputLayout{{
        { "float2", DXGI_FORMAT_R32G32_FLOAT, 8, "POSITION", 0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT, 8, "TEXCOORD", 0, 0, false },
    }},
    // Layout 6
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "POSITION", 0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "NORMAL",   0, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TANGENT",  0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "TEXCOORD", 0, 0, false },
    }},
    // Layout 7
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "NORMAL",   0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 1, false },
    }},
    // Layout 8
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 1, false },
    }},
    // Layout 9
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION", 0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD", 0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "NORMAL",   0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",  0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "COLOR",    0, 0, false },
    }},
    // Layout 10
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "POSITION", 0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "NORMAL",   0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "TEXCOORD", 1, 0, false },
    }},
    // Layout 11
    TigerInputLayout{{
        { "float2", DXGI_FORMAT_R32G32_FLOAT,     8, "POSITION", 0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "TEXCOORD", 0, 0, false },
    }},
    // Layout 12
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,     8, "TEXCOORD", 0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "NORMAL",   0, 0, false },
    }},
    // Layout 13
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "POSITION",     0, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "NORMAL",       0, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TANGENT",      0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,        4, "TEXCOORD",     0, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "BLENDWEIGHT",  0, 2, false },
        { "uint4",  DXGI_FORMAT_R8G8B8A8_UINT,       4, "BLENDINDICES", 0, 2, false },
    }},
    // Layout 14
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "POSITION",     0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "NORMAL",       0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TANGENT",      0, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     0, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     1, 0, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD",     2, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD",     3, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD",     4, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "TEXCOORD",     5, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "TEXCOORD",     6, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "TEXCOORD",     7, 1, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "BLENDINDICES", 0, 2, false },
    }},
    // Layout 15
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "POSITION",     0, 0, false },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "TEXCOORD",     0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "TEXCOORD",     5, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "TEXCOORD",     6, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "TEXCOORD",     7, 1, true  },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,        8, "BINORMAL",     0, 2, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "BLENDINDICES", 0, 3, false },
    }},
    // Layout 16
    TigerInputLayout{{
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT, 12, "POSITION", 0, 0, false },
    }},
    // Layout 17
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "POSITION", 0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD", 0, 1, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "NORMAL",   0, 1, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_FLOAT,  8, "TEXCOORD", 1, 1, false },
    }},
    // Layout 18
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION",     0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD",     0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "NORMAL",       0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TANGENT",      0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TEXCOORD",     5, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     6, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     7, 1, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "BLENDINDICES", 0, 3, false },
    }},
    // Layout 19
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION",     0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "NORMAL",       0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TANGENT",      0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD",     0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_FLOAT, 8, "TEXCOORD",     1, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TEXCOORD",     5, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     6, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     7, 1, true  },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,       8, "BINORMAL",     0, 2, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT,16, "BLENDINDICES", 0, 3, false },
    }},
    // Layout 20
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "POSITION",     0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "NORMAL",       0, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TANGENT",      0, 0, false },
        { "float2", DXGI_FORMAT_R16G16_SNORM,       4, "TEXCOORD",     0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_FLOAT, 8, "TEXCOORD",     1, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_FLOAT, 8, "TEXCOORD",     2, 0, false },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     3, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM, 8, "TEXCOORD",     5, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     6, 1, true  },
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,     4, "TEXCOORD",     7, 1, true  },
        { "float2", DXGI_FORMAT_R32G32_FLOAT,       8, "BINORMAL",     0, 2, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT,16, "BLENDINDICES", 0, 3, false },
    }},
    // Layout 21
    TigerInputLayout{{
        { "float4", DXGI_FORMAT_R8G8B8A8_UNORM,      4, "POSITION", 0, 0, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD", 0, 1, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "NORMAL",   0, 1, false },
        { "float4", DXGI_FORMAT_R32G32B32A32_FLOAT, 16, "TEXCOORD", 1, 1, false },
        { "float3", DXGI_FORMAT_R32G32B32_FLOAT,    12, "TEXCOORD", 2, 1, false },
    }},
    // Layout 22
    TigerInputLayout{{
        { "int4",   DXGI_FORMAT_R16G16B16A16_SINT,   8, "POSITION", 0, 0, false },
        { "float4", DXGI_FORMAT_R16G16B16A16_SNORM,  8, "NORMAL",   0, 1, false },
        { "float2", DXGI_FORMAT_R16G16_FLOAT,        4, "TEXCOORD", 1, 1, false },
    }},
};




HRESULT CreateInputLayoutFromTigerLayout(ID3D11Device* device, const TigerInputLayout& layout, Microsoft::WRL::ComPtr<ID3D11InputLayout>& outLayout)
{
    std::vector<D3D11_INPUT_ELEMENT_DESC> elems;
    elems.reserve(layout.elements.size());
    std::string shaderSrc = "struct s_vs_in { ";
    for (size_t i = 0; i < layout.elements.size(); ++i)
    {
        const auto& e = layout.elements[i];

        // Append to HLSL like: "float3 v0 : POSITION0; "
        shaderSrc += std::format(
            "{} v{} : {}{}; ",
            e.hlsl_type,
            i,
            e.semantic_name,
            e.semantic_index
        );

        D3D11_INPUT_ELEMENT_DESC desc = {};
        desc.SemanticName = e.semantic_name.c_str();
        desc.SemanticIndex = e.semantic_index;
        desc.Format = e.DxgiFormat;
        desc.InputSlot = e.buffer_index;
        desc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        desc.InputSlotClass = e.is_instance_data ? D3D11_INPUT_PER_INSTANCE_DATA
            : D3D11_INPUT_PER_VERTEX_DATA;
        desc.InstanceDataStepRate = e.is_instance_data ? 1u : 0u;

        elems.push_back(desc);
    }
    shaderSrc += "}; float4 vs(s_vs_in input) : SV_POSITION { return float4(0,0,0,0); }";
    Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
    Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;
    const UINT compileFlags = 0;
    HRESULT hr = D3DCompile(
        shaderSrc.data(),
        shaderSrc.size(),
        "create_vertex_declaration_inline", // optional source name
        nullptr,                            // macros
        nullptr,                            // include
        "vs",                               // entry point
        "vs_5_0",                           // target
        compileFlags,
        0,
        shaderBlob.GetAddressOf(),
        errorBlob.GetAddressOf());
    if (FAILED(hr))
    {
        ErrorLogger::Log(hr, L"Failed to create VS from TigerInputLayout");
        return hr;
    }
    hr = device->CreateInputLayout(
        elems.data(),
        static_cast<UINT>(elems.size()),
        shaderBlob->GetBufferPointer(),
        shaderBlob->GetBufferSize(),
        outLayout.GetAddressOf());
    if (FAILED(hr))
    {
        ErrorLogger::Log(hr, L"Failed to create CreateInputLayoutS from TigerInputLayout");
        return hr;
    }
    return hr;
}