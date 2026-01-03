
#pragma once
#include <array>
#include <vector>
#include <string>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "TigerEngine/Technique/blend_state_descs.h"
#include "TigerEngine/Technique/rasterizer_states.h"
#include "TigerEngine/Technique/depth_bias_states.h"
#include "TigerEngine/Technique/depth_stencil_states.h"

#pragma comment(lib, "d3dcompiler.lib")

using Microsoft::WRL::ComPtr;



static constexpr std::array<std::pair<uint8_t, uint8_t>, 89> DEPTH_STENCIL_COMBOS = { {
    {0, 0},    
    {1, 1},
    {2, 1},    
    {8, 1},
    {2, 2},    
    {1, 3},
    {1, 4},    
    {2, 5},
    {2, 6},    
    {2, 9},
    {2, 10},   
    {2, 0x0B},
    {2, 0x0C}, 
    {4, 1},
    {6, 1},    
    {3, 1},
    {7, 1},    
    {3, 0x10},
    {9, 0x10},
    {3, 0x11},
    {3, 0x12},
    {7, 0x13},
    {7, 0x1B},
    {3, 0x13},
    {3, 0x19},
    {3, 0x1B},
    {6, 0x14},
    {2, 0x15},
    {3, 0x15},
    {3, 0x18},
    {3, 0x1A},
    {1, 0x1D},
    {1, 0x12},
    {1, 0x13},
    {10, 1},
    {0x0B, 1},
    {3, 0x1E},
    {0x0C, 0x1F},
    {1, 0x1F},
    {1, 0x20},
    {1, 0x21},
    {3, 0x21},
    {2, 0x21},
    {6, 0x20},
    {3, 0x20},
    {3, 6},
    {3, 10},
    {3, 0x0B},
    {3, 0x0C},
    {3, 9},
    {0x0D, 0x22},
    {1, 0x23},
    {3, 0x1C},
    {7, 0x1C},
    {0x0D, 0x10},
    {0x0D, 0x25},
    {9, 0x24},
    {3, 0x26},
    {1, 0x26},
    {3, 0x27},
    {1, 0x27},
    {3, 0x14},
    {1, 0x14},
    {3, 0x28},
    {3, 8},
    {2, 8},
    {1, 2},
    {1, 8},
    {3, 7},
    {3, 0x17},
    {3, 0x0D},
    {3, 0x0E},
    {3, 0x0F},
    {1, 0x29},
    {1, 0x2A},
    {1, 0x2B},
    {1, 0x2C},
    {1, 0x2D},
    {1, 0x2E},
    {1, 0x2F},
    {1, 0x30},
    {1, 0x1A},
    {2, 0x16},
    {5, 1},
    {5, 0x29},
    {5, 0x2A},
    {10, 0x16},
    {1, 0x16},
    {14, 1},   
} };

static_assert(DEPTH_STENCIL_COMBOS.size() == 89, "Combo count mismatch");

struct RenderStates
{
    std::array<ComPtr<ID3D11BlendState>, 90> blend_states;
    
    std::vector<ComPtr<ID3D11RasterizerState>> rasterizer_states; 
    std::array<ComPtr<ID3D11DepthStencilState>, 89> depth_stencil_states;

    static HRESULT Create(ID3D11Device* device, RenderStates& out);
};
