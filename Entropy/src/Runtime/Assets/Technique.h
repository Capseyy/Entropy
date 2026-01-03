#pragma once
#include <wrl/client.h>
#include <d3d11.h>
#include <vector>
#include <cstdint>
#include <memory>
#include "TigerEngine/Technique/tfx/tfx.h"
#include "TigerEngine/Technique/technique.h"
#include "Renderer/Graphics/RenderStates.h"
#include "TigerEngine/ClientStartup/RenderGlobals.h"
#include "Renderer/Loaders/Scope.h"

namespace EntropyAssets {           

    struct BufferSRVRes {
        Microsoft::WRL::ComPtr<ID3D11Buffer>            buffer;
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
    };

    struct Texture2DRes { 
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        ID3D11ShaderResourceView* Get() const { return srv.Get(); }
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t arraySize = 1;

    };

    struct Texture3DRes { 
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
        ID3D11ShaderResourceView* Get() const { return srv.Get(); }
    };


    struct TexBinding {
        UINT slot = 0;                                  
        std::shared_ptr<Texture2DRes> tex;              
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
       
        std::vector<std::shared_ptr<VertexShader>>   VS;
        std::vector<std::shared_ptr<PixelShader>>    PS;
        std::vector<std::shared_ptr<ComputeShader>>  CS;
        std::vector<std::shared_ptr<GeometryShader>> GS;
        std::vector<std::shared_ptr<HullShader>>     HS;
        std::vector<std::shared_ptr<DomainShader>>   DS;

  
        std::vector<std::shared_ptr<Texture2DRes>>   Textures;
        std::vector<UINT>                            psTextureSlots;

        std::vector<std::shared_ptr<Texture3DRes>>   Textures3D;
        std::vector<UINT>                            psTextureSlots3D;

     
        std::vector<std::shared_ptr<SamplerRes>>     Samplers;
        std::vector<UINT>                            psSamplerSlots;  

        std::vector<std::shared_ptr<CBufferRes>>     CBuffers;
        std::vector<UINT>                            psCBSlots;       

        std::shared_ptr<CBufferRes>                  CBuffers_fallback = nullptr;
        UINT                                         psCBSlots_fallback;

        
        std::vector<std::shared_ptr<Texture2DRes>>   Textures_VS;
        std::vector<UINT>                            vsTextureSlots;

        std::vector<std::shared_ptr<Texture3DRes>>   Textures3D_VS;
        std::vector<UINT>                            vsTextureSlots3D;

 
        std::vector<std::shared_ptr<SamplerRes>>     Samplers_VS;
        std::vector<UINT>                            vsSamplerSlots;  

       
        std::vector<std::shared_ptr<CBufferRes>>     CBuffers_VS;
        std::vector<UINT>                            vsCBSlots;      

        std::shared_ptr<CBufferRes>                  CBuffers_fallback_VS = nullptr;
        UINT                                         vsCBSlots_fallback;

        uint64_t usedScopes;

        STechniqueShader vertexdata;
        STechniqueShader pixeldata;

        uint32_t StateSelection;
        
        bool Bind(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes);
        bool Bind_With_Channels(Microsoft::WRL::ComPtr<ID3D11Device> pDevice, Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes, std::unordered_map<uint32_t, Vec4> channels);
        bool Bind_Only_PS(Microsoft::WRL::ComPtr<ID3D11Device> pDevice,
            Microsoft::WRL::ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs, RenderStates& states, std::vector<std::pair<std::string, TigerScope>>& scopes, std::unordered_map<uint32_t, float_t> channels);
        
    };

} 
