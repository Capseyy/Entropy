#include "TigerEngine/Technique/technique.h"

Material STechnique::Initialize(ID3D11Device* device, ID3D11DeviceContext* deviceContext, D3D11_INPUT_ELEMENT_DESC* desc, UINT numElements)
{
	Material mat;
	mat.vs.tag = TagHash(this->VertexShader.ShaderTag.reference);
	mat.vs.Initialize(device, desc, numElements);
	printf("Initialized VS with tag 0x%X\n", mat.vs.tag.hash);


	mat.ps.tag = TagHash(this->PixelShader.ShaderTag.reference);
	mat.ps.Initialize(device);
	printf("Initialized PS\n");
	for (auto& ps_tex : this->PixelShader.Textures)
	{
		mat.ps.ps_texs.push_back(TagHash(ps_tex.Texture.tagHash32));

	}
	printf("Initialized ps with tag 0x%X\n", mat.ps.tag.hash);
	printf("Initialized Technique\n");
	printf("Technique VS Tag: 0x%X PS Tag: 0x%X\n", mat.vs.tag.hash, mat.ps.tag.hash);
	return mat;
}