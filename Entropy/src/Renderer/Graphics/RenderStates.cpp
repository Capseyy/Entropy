// RenderStates.cpp
#include "RenderStates.h"
#include <cassert>

static D3D11_DEPTH_WRITE_MASK ToWriteMask(UINT8 w)
{
    return (w != 0) ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
}


HRESULT RenderStates::Create(ID3D11Device* device, RenderStates& out)
{
    if (!device) return E_INVALIDARG;

    // ---- Blend states (90) ----
    for (size_t i = 0; i < out.blend_states.size(); ++i) {
        D3D11_BLEND_DESC bd{};
        bd.AlphaToCoverageEnable  = FALSE;
        bd.IndependentBlendEnable = TRUE;

        // First 4 from table
        for (int rt = 0; rt < 4; ++rt)
            bd.RenderTarget[rt] = BLEND_STATE_DESCS[i].RenderTarget[rt];

        // Extend by repeating slot 3 for RT[4..7], matching your Rust behavior
        for (int rt = 4; rt < 8; ++rt)
            bd.RenderTarget[rt] = BLEND_STATE_DESCS[i].RenderTarget[3];

        HRESULT hr = device->CreateBlendState(&bd, out.blend_states[i].ReleaseAndGetAddressOf());
        if (FAILED(hr)) return hr;
    }

	out.rasterizer_states=CompileRasterizerStates(device);
    constexpr size_t kBlendCount = std::size(BLEND_STATE_DESCS); // 0x5a == 90
    static_assert(kBlendCount == 0x5a, "Blend state table size mismatch");

    // Ensure the destination container can hold them all

    for (size_t i = 0; i < kBlendCount; ++i)
    {
        D3D11_BLEND_DESC bd = {};
        bd.AlphaToCoverageEnable = FALSE;   // keep as-is (table doesn't carry this)
        bd.IndependentBlendEnable = TRUE;

        // Copy RT[0..3] from the table
        for (int rt = 0; rt < 4; ++rt)
        {
            bd.RenderTarget[rt].BlendEnable = BLEND_STATE_DESCS[i].RenderTarget[rt].BlendEnable;
            bd.RenderTarget[rt].SrcBlend = BLEND_STATE_DESCS[i].RenderTarget[rt].SrcBlend;
            bd.RenderTarget[rt].DestBlend = BLEND_STATE_DESCS[i].RenderTarget[rt].DestBlend;
            bd.RenderTarget[rt].BlendOp = BLEND_STATE_DESCS[i].RenderTarget[rt].BlendOp;
            bd.RenderTarget[rt].SrcBlendAlpha = BLEND_STATE_DESCS[i].RenderTarget[rt].SrcBlendAlpha;
            bd.RenderTarget[rt].DestBlendAlpha = BLEND_STATE_DESCS[i].RenderTarget[rt].DestBlendAlpha;
            bd.RenderTarget[rt].BlendOpAlpha = BLEND_STATE_DESCS[i].RenderTarget[rt].BlendOpAlpha;
            bd.RenderTarget[rt].RenderTargetWriteMask = BLEND_STATE_DESCS[i].RenderTarget[rt].RenderTargetWriteMask;
        }

        // Repeat slot 3 for RT[4..7] (matches your Rust behavior)
        for (int rt = 4; rt < 8; ++rt)
        {
            bd.RenderTarget[rt] = bd.RenderTarget[3];
        }

        HRESULT hr = device->CreateBlendState(&bd, out.blend_states[i].ReleaseAndGetAddressOf());
        if (FAILED(hr)) return hr;
    }

    // TODO: create rasterizer/depth-stencil states here as needed.
    

    // ---- DepthStencil (89 pairs) ----
   
    

    return S_OK;
}
