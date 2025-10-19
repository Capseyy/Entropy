#include "input_layout.h"
#include <d3dcompiler.h>
#include "Renderer/Tools/ErrorLogger.h"
#include <format>



static void AppendSemantic(std::string& hlsl,
    const std::string& type,
    const std::string& sem, uint32_t idx,
    size_t vindex)
{
    // e.g. "float3 v0 : POSITION0;"
    hlsl += std::format("{} v{} : {}{}; ", type, vindex, sem, idx);
}

HRESULT CreateInputLayoutFromTigerLayout(
    ID3D11Device* device,
    const TigerInputLayout& layout,
    Microsoft::WRL::ComPtr<ID3D11InputLayout>& outLayout)
{
    // 1) Build a minimal VS HLSL with the exact input signature
    std::string hlsl = "struct VSIN { ";
    for (size_t i = 0; i < layout.elements.size(); ++i) {
        const auto& e = layout.elements[i];
        AppendSemantic(hlsl, e.hlsl_type, e.semantic_name, e.semantic_index, i);
    }
    hlsl += "}; float4 main(VSIN i) : SV_POSITION { return float4(0,0,0,1); }";

    Microsoft::WRL::ComPtr<ID3DBlob> vsBlob, err;
    UINT flags = 0;
#ifdef _DEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif
    HRESULT hr = D3DCompile(hlsl.data(), hlsl.size(),
        "TigerILGen", nullptr, nullptr,
        "main", "vs_5_0", flags, 0,
        vsBlob.GetAddressOf(), err.GetAddressOf());
    if (FAILED(hr)) {
        if (err) OutputDebugStringA((const char*)err->GetBufferPointer());
        ErrorLogger::Log(hr, L"D3DCompile failed in CreateInputLayoutFromTigerLayout");
        return hr;
    }

    // 2) Convert to D3D descs (note: names must be alive during the call)
    std::vector<std::string> nameHold;
    nameHold.reserve(layout.elements.size());
    std::vector<D3D11_INPUT_ELEMENT_DESC> elems;
    elems.reserve(layout.elements.size());

    for (const auto& e : layout.elements) {
        nameHold.push_back(e.semantic_name);
        D3D11_INPUT_ELEMENT_DESC d{};
        d.SemanticName = nameHold.back().c_str();
        d.SemanticIndex = e.semantic_index;
        d.Format = e.DxgiFormat;
        d.InputSlot = e.buffer_index;
        d.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
        d.InputSlotClass = e.is_instance_data ? D3D11_INPUT_PER_INSTANCE_DATA
            : D3D11_INPUT_PER_VERTEX_DATA;
        d.InstanceDataStepRate = e.is_instance_data ? 1u : 0u;
        elems.push_back(d);
    }

    hr = device->CreateInputLayout(elems.data(),
        static_cast<UINT>(elems.size()),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        outLayout.GetAddressOf());
    if (FAILED(hr)) {
        ErrorLogger::Log(hr, L"CreateInputLayout failed");
    }
    return hr;
}
