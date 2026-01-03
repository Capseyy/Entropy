#include "Sampler.h"

std::optional<D3D11_SAMPLER_DESC> BuildSamplerDescFromTag(TagHash tag)
{
    auto sampTag = bin::parse<UT_SamplerRaw>(tag.data, tag.size, bin::Endian::Little);

    D3D11_SAMPLER_DESC sd = {};
    
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
    return sd;
}
