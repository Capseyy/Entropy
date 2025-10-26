#pragma once
#include <array>
#include <d3d11.h>

// If you put BungieRasterizerDesc/ToD3D in another header, include it here.
// #include "rasterizer_states.hpp"

struct BungieDepthBiasDesc {
    int   depth_bias;   // D3D11_RASTERIZER_DESC::DepthBias
    float slope_scale;  // D3D11_RASTERIZER_DESC::SlopeScaledDepthBias
    float clamp;        // D3D11_RASTERIZER_DESC::DepthBiasClamp
};

// region Depth Biases
inline constexpr std::array<BungieDepthBiasDesc, 9> DEPTH_BIASES = { {
        // 0
        { 0,   0.0f, 0.0f },
        // 1
        { 0,   0.0f, 0.0f },
        // 2
        { 5,   2.0f, 10000000000.0f },
        // 3
        { 10,  4.0f, 10000000000.0f },
        // 4
        { 15,  6.0f, 10000000000.0f },
        // 5
        { 20,  8.0f, 10000000000.0f },
        // 6
        { 2,   2.0f, 10000000000.0f },
        // 7
        { -1, -2.0f, 10000000000.0f },
        // 8
        { 51,  2.0f, 10000000000.0f },
    } };