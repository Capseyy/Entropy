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
    

    // ---- DepthStencil (89 pairs) ----
   
    

    return S_OK;
}
