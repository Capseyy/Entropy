
#pragma once
#include <array>
#include <vector>
#include <d3d11.h>
#include <wrl/client.h>

struct BungieRasterizerDesc {
    D3D11_FILL_MODE fill_mode;
    D3D11_CULL_MODE cull_mode;
    BOOL            front_counter_clockwise;
    BOOL            depth_clip_enable;
    BOOL            scissor_enable;
};


inline D3D11_RASTERIZER_DESC ToD3D(const BungieRasterizerDesc& src) {
    D3D11_RASTERIZER_DESC d{};
    d.FillMode = src.fill_mode;
    d.CullMode = src.cull_mode;
    d.FrontCounterClockwise = src.front_counter_clockwise;
    d.DepthBias = 0;
    d.DepthBiasClamp = 0.0f;
    d.SlopeScaledDepthBias = 0.0f;
    d.DepthClipEnable = src.depth_clip_enable;
    d.ScissorEnable = src.scissor_enable;
    d.MultisampleEnable = FALSE;
    d.AntialiasedLineEnable = FALSE;
    return d;
}


inline constexpr std::array<BungieRasterizerDesc, 9> RASTERIZER_STATES = { {
        
        { D3D11_FILL_SOLID,    D3D11_CULL_NONE,  TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_NONE,  TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_BACK,  TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_FRONT, TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_WIREFRAME,D3D11_CULL_BACK,  TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_WIREFRAME,D3D11_CULL_NONE,  TRUE,  TRUE,  FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_BACK,  TRUE,  FALSE, FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_NONE,  TRUE,  FALSE, FALSE },
        
        { D3D11_FILL_SOLID,    D3D11_CULL_FRONT, TRUE,  FALSE, FALSE },
    } };



inline std::vector<Microsoft::WRL::ComPtr<ID3D11RasterizerState>>
CompileRasterizerStates(ID3D11Device* device) {
    using Microsoft::WRL::ComPtr;
    std::vector<ComPtr<ID3D11RasterizerState>> out;
    out.reserve(RASTERIZER_STATES.size());
    for (const auto& rs : RASTERIZER_STATES) {
        D3D11_RASTERIZER_DESC d3d = ToD3D(rs);
        ComPtr<ID3D11RasterizerState> state;
        if (SUCCEEDED(device->CreateRasterizerState(&d3d, &state))) {
            out.emplace_back(std::move(state));
        }
        else {
            out.emplace_back(nullptr);
        }
    }
    return out;
}
