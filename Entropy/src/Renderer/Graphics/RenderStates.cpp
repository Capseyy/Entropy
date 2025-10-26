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

    // ---- Rasterizer × DepthBias (9 × 9) ----
    for (size_t db = 0; db < out.rasterizer_states.size(); ++db) {
        for (size_t rs = 0; rs < out.rasterizer_states[db].size(); ++rs) {
            const auto& rsd = RASTERIZER_STATES[rs];
            const auto& dbd = DEPTH_BIASES[db];

            D3D11_RASTERIZER_DESC desc{};
            desc.FillMode              = rsd.fill_mode;
            desc.CullMode              = rsd.cull_mode;
            desc.FrontCounterClockwise = rsd.front_counter_clockwise;
            desc.DepthBias             = dbd.depth_bias;
            desc.DepthBiasClamp        = dbd.clamp;
            desc.SlopeScaledDepthBias  = dbd.slope_scale;
            desc.DepthClipEnable       = rsd.depth_clip_enable;
            desc.ScissorEnable         = rsd.scissor_enable;
            desc.MultisampleEnable     = FALSE;
            desc.AntialiasedLineEnable = FALSE;

            HRESULT hr = device->CreateRasterizerState(&desc, out.rasterizer_states[db][rs].ReleaseAndGetAddressOf());
            if (FAILED(hr)) return hr;
        }
    }

    // ---- DepthStencil (89 pairs) ----
    for (size_t i = 0; i < out.depth_stencil_states.size(); ++i) {
        const auto [depthIdx, stencilIdx] = DEPTH_STENCIL_COMBOS[i];
        const auto& depth   = DEPTH_STATES[depthIdx];
        const auto& stencil = STENCIL_STATES[stencilIdx];

        D3D11_DEPTH_STENCIL_DESC d{};
        d.DepthEnable      = depth.enable;
        d.DepthWriteMask   = ToWriteMask(depth.write_mask);
        d.DepthFunc        = depth.func;        // first variant
        d.StencilEnable    = stencil.stencil_enable;
        d.StencilReadMask  = stencil.stencil_read_mask;
        d.StencilWriteMask = stencil.stencil_write_mask;

        d.FrontFace.StencilFunc        = stencil.front_face.func;
        d.FrontFace.StencilPassOp      = stencil.front_face.pass_op;
        d.FrontFace.StencilFailOp      = stencil.front_face.fail_op;
        d.FrontFace.StencilDepthFailOp = stencil.front_face.depth_fail_op;

        d.BackFace.StencilFunc         = stencil.back_face.func;
        d.BackFace.StencilPassOp       = stencil.back_face.pass_op;
        d.BackFace.StencilFailOp       = stencil.back_face.fail_op;
        d.BackFace.StencilDepthFailOp  = stencil.back_face.depth_fail_op;

        HRESULT hr = device->CreateDepthStencilState(&d, out.depth_stencil_states[i].first.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return hr;

        // Second variant differs only by DepthFunc (matches your Rust code)
        d.DepthFunc = depth.func_alt;
        hr = device->CreateDepthStencilState(&d, out.depth_stencil_states[i].second.ReleaseAndGetAddressOf());
        if (FAILED(hr)) return hr;
    }

    return S_OK;
}
