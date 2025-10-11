#include "input_layout.h"

HRESULT CreateInputLayoutFromTigerLayout(ID3D11Device* device, const TigerInputLayout& layout, Microsoft::WRL::ComPtr<ID3D11InputLayout>& outLayout, D3D11_INPUT_ELEMENT_DESC& outDesc)
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

    return hr;
}