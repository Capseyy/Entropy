#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <memory>
#include "TigerEngine/Technique/tfx/tfx.h"
#include "TigerEngine/Technique/technique.h"
#include "Renderer/Graphics/RenderStates.h"

namespace EntropyAssets {           // <<< add

    struct BufferSRVRes {
        Microsoft::WRL::ComPtr<ID3D11Buffer>            buffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct Texture2DRes { // whatever you already have
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        ID3D11ShaderResourceView* Get() const { return srv.Get(); }
    };

    struct Texture3DRes { // whatever you already have
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        ID3D11ShaderResourceView* Get() const { return srv.Get(); }
    };


    struct TexBinding {
        UINT slot = 0;                                  // t-slot
        std::shared_ptr<Texture2DRes> tex;              // SRV wrapper
    };

    using Microsoft::WRL::ComPtr;

    struct VertexShader { ComPtr<ID3D11VertexShader> vs; ComPtr<ID3D11InputLayout> layout; };
    struct PixelShader { ComPtr<ID3D11PixelShader>  ps; };
    struct ComputeShader { ComPtr<ID3D11ComputeShader> cs; };
    struct GeometryShader { ComPtr<ID3D11GeometryShader> gs; };
    struct HullShader { ComPtr<ID3D11HullShader>   hs; };
    struct DomainShader { ComPtr<ID3D11DomainShader> ds; };
    struct SamplerRes { ComPtr<ID3D11SamplerState> sampler; };
    struct CBufferRes { ComPtr<ID3D11Buffer> buffer; UINT size = 0; bool tfx_buffer = false; };

    struct Technique {
        uint32_t id = 0;

        // shaders
        std::vector<std::shared_ptr<VertexShader>>   VS;
        std::vector<std::shared_ptr<PixelShader>>    PS;
        std::vector<std::shared_ptr<ComputeShader>>  CS;
        std::vector<std::shared_ptr<GeometryShader>> GS;
        std::vector<std::shared_ptr<HullShader>>     HS;
        std::vector<std::shared_ptr<DomainShader>>   DS;

        // PS textures
        std::vector<std::shared_ptr<Texture2DRes>>   Textures;
        std::vector<UINT>                            psTextureSlots;

        std::vector<std::shared_ptr<Texture3DRes>>   Textures3D;
        std::vector<UINT>                            psTextureSlots3D;

        // NEW: PS samplers
        std::vector<std::shared_ptr<SamplerRes>>     Samplers;
        std::vector<UINT>                            psSamplerSlots;   // <— add

        // NEW: PS cbuffer (you can have more than one if your format allows)
        std::vector<std::shared_ptr<CBufferRes>>     CBuffers;
        std::vector<UINT>                            psCBSlots;        // <— add

        std::shared_ptr<CBufferRes>                  CBuffers_fallback = nullptr;
        UINT                                         psCBSlots_fallback;

        STechniqueShader vertexdata;
        STechniqueShader pixeldata;

        uint32_t StateSelection;
        
        bool Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage externs, RenderStates states);
    };

} // namespace EntropyAssets       // <<< add
