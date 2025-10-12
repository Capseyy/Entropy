#include "material.h"


void Material::InitializeCBuffer(ID3D11Device* device, UINT byteWidth, TagHash cbuffer)
{

	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = (UINT)cbuffer.size;
	bd.Usage = D3D11_USAGE_DEFAULT;   // or D3D11_USAGE_IMMUTABLE if never changes
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA init{};
	init.pSysMem = cbuffer.data;               // can be smaller than ByteWidth; driver reads blobSize

	HRESULT hr = device->CreateBuffer(&bd, &init, &this->cbuffer_ps);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create constant buffer for material.");
	}
}