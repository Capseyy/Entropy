#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <memory>

namespace EntropyAssets {           // <<< add

    using Microsoft::WRL::ComPtr;

    struct VertexShader { ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11InputLayout> layout; };
    struct PixelShader { ComPtr<ID3D11PixelShader>  ps; };
    struct ComputeShader { ComPtr<ID3D11ComputeShader> cs; };
    struct GeometryShader { ComPtr<ID3D11GeometryShader> gs; };
    struct HullShader { ComPtr<ID3D11HullShader>   hs; };
    struct DomainShader { ComPtr<ID3D11DomainShader> ds; };

    struct Texture2DRes { ComPtr<ID3D11Texture2D> tex; ComPtr<ID3D11ShaderResourceView> srv; };
    struct SamplerRes { ComPtr<ID3D11SamplerState> sampler; };
    struct CBufferRes { ComPtr<ID3D11Buffer> buffer; UINT size = 0; };

    struct Technique {
        uint32_t id = 0;
        std::vector<std::shared_ptr<VertexShader>>   VS;
        std::vector<std::shared_ptr<PixelShader>>    PS;
        std::vector<std::shared_ptr<ComputeShader>>  CS;
        std::vector<std::shared_ptr<GeometryShader>> GS;
        std::vector<std::shared_ptr<HullShader>>     HS;
        std::vector<std::shared_ptr<DomainShader>>   DS;

        std::vector<std::shared_ptr<Texture2DRes>>   Textures;
        std::vector<std::shared_ptr<SamplerRes>>     Samplers;
        std::vector<std::shared_ptr<CBufferRes>>     CBuffers;
    };

} // namespace EntropyAssets       // <<< add
