#include "TigerEngine/Technique/technique.h"

void STechnique::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements)
{
	Material mat;

	if (this->VertexShader.ShaderTag.hash != 0xFFFFFFFF) 
	{
		D2VertexShader vs;
		vs.tag = TagHash(this->VertexShader.ShaderTag.reference);
	}
	if (this->PixelShader.ShaderTag.hash != 0xFFFFFFFF)
	{
		D2PixelShader ps;
		ps.tag = TagHash(this->PixelShader.ShaderTag.reference);
		for (auto &ps_tex : this->PixelShader.Textures)
		{
			ps.ps_texs.push_back(TagHash(ps_tex.Texture.tagHash32));

		}
	}
	printf("Initialized Technique\n");
}