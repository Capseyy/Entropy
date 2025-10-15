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

void Material::InitializeCBufferFallback(ID3D11Device* device, UINT byteWidth, std::vector<Unk_90008080> fallbackBytes)
{
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = (UINT)fallbackBytes.size()*16;
	bd.Usage = D3D11_USAGE_DEFAULT;   // or D3D11_USAGE_IMMUTABLE if never changes
	bd.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA init{};
	init.pSysMem = fallbackBytes.data();               // can be smaller than ByteWidth; driver reads blobSize

	HRESULT hr = device->CreateBuffer(&bd, &init, &this->cbuffer_ps_fallback);
	if (FAILED(hr))
	{
		throw std::runtime_error("Failed to create constant buffer for material.");
	}
}

void Material::InitializeSampler(ID3D11Device* device, UT_SamplerRaw sampTag)
{
    if (!device) throw std::runtime_error("InitializeSampler: device is null");

    D3D11_SAMPLER_DESC sd = {};
    // Cast raw numeric enums from your game data to D3D11 types (with light sanitization)
    sd.Filter = static_cast<D3D11_FILTER>(sampTag.Filter);
    sd.AddressU = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampTag.AddressU);
    sd.AddressV = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampTag.AddressV);
    sd.AddressW = static_cast<D3D11_TEXTURE_ADDRESS_MODE>(sampTag.AddressW);

    sd.MipLODBias = sampTag.MipLODBias;
    sd.MaxAnisotropy = std::clamp(sampTag.MaxAnisotropy, 1u, 16u);

    sd.ComparisonFunc = static_cast<D3D11_COMPARISON_FUNC>(sampTag.ComparisonFunc);

    sd.BorderColor[0] = sampTag.BorderColor[0];
    sd.BorderColor[1] = sampTag.BorderColor[1];
    sd.BorderColor[2] = sampTag.BorderColor[2];
    sd.BorderColor[3] = sampTag.BorderColor[3];

    sd.MinLOD = sampTag.MinLOD;
    sd.MaxLOD = (sampTag.MaxLOD <= 0.0f) ? D3D11_FLOAT32_MAX : sampTag.MaxLOD;

    // If filter isn’t anisotropic, driver ignores MaxAnisotropy; if it IS comparison filter, use SampleCmp in HLSL.

    HRESULT hr = device->CreateSamplerState(&sd, &sampler);
    if (FAILED(hr))
        throw std::runtime_error("Failed to create sampler state.");
}