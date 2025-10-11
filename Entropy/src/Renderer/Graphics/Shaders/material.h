#pragma once
#include "Shaders.h"
#include <vector>
#include "d3d11.h"
#include "wrl/client.h"

class Material
{
public:
	D2VertexShader vs;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> vs_textures;
	D2PixelShader ps;
	std::vector<Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> ps_textures;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cbuffer_ps;
};

