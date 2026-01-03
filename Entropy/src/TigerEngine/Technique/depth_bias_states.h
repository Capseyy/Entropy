#pragma once
#include <array>
#include <d3d11.h>




struct BungieDepthBiasDesc {
    int   depth_bias;   
    float slope_scale;  
    float clamp;        
};


inline constexpr std::array<BungieDepthBiasDesc, 9> DEPTH_BIASES = { {
        
        { 0,   0.0f, 0.0f },
        
        { 0,   0.0f, 0.0f },
        
        { 5,   2.0f, 10000000000.0f },
        
        { 10,  4.0f, 10000000000.0f },
        
        { 15,  6.0f, 10000000000.0f },
        
        { 20,  8.0f, 10000000000.0f },
        
        { 2,   2.0f, 10000000000.0f },
        
        { -1, -2.0f, 10000000000.0f },
        
        { 51,  2.0f, 10000000000.0f },
    } };