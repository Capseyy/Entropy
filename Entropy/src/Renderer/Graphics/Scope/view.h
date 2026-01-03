#pragma once

#include <cstddef>
#include <DirectXMath.h>
#include "d3d11.h"
#include "wrl/client.h"
using namespace DirectX;


struct Viewport {
    XMFLOAT2 size; 
    
    XMMATRIX target_pixel_to_projective() const;
};

inline Microsoft::WRL::ComPtr<ID3D11Buffer> g_scopeView_b12;

struct alignas(16) View
{
    
    float      resolution_width;                 
    float      resolution_height;                
    float      _pad08[2] = {};                   

    
    XMFLOAT4   view_miscellaneous = { 0.0f,1.0f,0.0f,0.0f };          
    XMFLOAT4   position = {};           
    XMFLOAT4   unk30 = {};           

    XMFLOAT4X4 world_to_camera;                  
    XMFLOAT4X4 camera_to_projective;             
    XMFLOAT4X4 camera_to_world;                  
    XMFLOAT4X4 projective_to_camera;             
    XMFLOAT4X4 world_to_projective;              
    XMFLOAT4X4 projective_to_world;              
    XMFLOAT4X4 target_pixel_to_world;            
    XMFLOAT4X4 target_pixel_to_camera;           
    XMFLOAT4X4 unk240;                           
    XMFLOAT4X4 tptow_no_proj_w;                  
    XMFLOAT4X4 unk2c0;                           

    
    static inline XMMATRIX load(const XMFLOAT4X4& m) { return XMLoadFloat4x4(&m); }
    static inline void     store(XMFLOAT4X4& dst, FXMMATRIX m) { XMStoreFloat4x4(&dst, m); }

    
    static inline XMMATRIX drop_translation(FXMMATRIX m) {
        XMMATRIX r = m;
        
        r.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        return r;
    }

    static inline DirectX::XMVECTOR GetColumn(const DirectX::XMMATRIX& M, int c)
    {
        using namespace DirectX;
        return XMVectorSet(M.r[0].m128_f32[c], M.r[1].m128_f32[c], M.r[2].m128_f32[c], M.r[3].m128_f32[c]);
    }

    static inline DirectX::XMMATRIX SetColumn(DirectX::XMMATRIX M, int c, DirectX::XMVECTOR v)
    {
        using namespace DirectX;
        M.r[0].m128_f32[c] = XMVectorGetX(v);
        M.r[1].m128_f32[c] = XMVectorGetY(v);
        M.r[2].m128_f32[c] = XMVectorGetZ(v);
        M.r[3].m128_f32[c] = XMVectorGetW(v);
        return M;
    }

    
    static inline DirectX::XMMATRIX DropTranslationRow(const DirectX::XMMATRIX& M)
    {
        using namespace DirectX;
        XMMATRIX R = M;
        R.r[3] = XMVectorSet(0.f, 0.f, 0.f, 1.f);
        return R;
    }

    
    static inline DirectX::XMMATRIX load_rm(const DirectX::XMFLOAT4X4& m) {
        using namespace DirectX;
        
        return XMMatrixTranspose(XMLoadFloat4x4(&m));
    }
    static inline void store_rm(DirectX::XMFLOAT4X4& dst, const DirectX::XMMATRIX& colM) {
        using namespace DirectX;
        
        XMStoreFloat4x4(&dst, XMMatrixTranspose(colM));
    }
    static inline DirectX::XMMATRIX drop_translation_col(const DirectX::XMMATRIX& M) {
        using namespace DirectX;
        
        XMMATRIX R = M;
        R.r[0].m128_f32[3] = 0.f;
        R.r[1].m128_f32[3] = 0.f;
        R.r[2].m128_f32[3] = 0.f;
        R.r[3].m128_f32[3] = 1.f;
        return R;
    }

    void derive_matrices_vs(const Viewport& viewport)
    {
        using namespace DirectX;

        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        
        const XMMATRIX V = load_rm(world_to_camera);        
        const XMMATRIX P = load_rm(camera_to_projective);   

        
        const XMMATRIX CW = XMMatrixInverse(nullptr, V);          
        const XMMATRIX WP = XMMatrixMultiply(V,P);               
        const XMMATRIX PC = XMMatrixInverse(nullptr, P);          
        const XMMATRIX PW = XMMatrixInverse(nullptr, WP);         

        
        store_rm(camera_to_world, CW);   
        store_rm(world_to_projective, WP);   
        store_rm(projective_to_camera, PC);  
        store_rm(projective_to_world, PW);   

        const XMMATRIX TPtP = viewport.target_pixel_to_projective(); 
        const XMMATRIX TPtP_col = XMMatrixTranspose(TPtP);           
        const XMMATRIX TPtC = XMMatrixMultiply(PC, TPtP_col);
        const XMMATRIX TPtW = XMMatrixMultiply(CW, TPtC);
        store_rm(target_pixel_to_camera, XMMatrixIdentity());
        store_rm(target_pixel_to_world, TPtW);

      
        {
            XMFLOAT4X4 cwRM; XMStoreFloat4x4(&cwRM, XMMatrixTranspose(CW)); 
            position = cwRM._41 ? XMFLOAT4(cwRM._41, cwRM._42, cwRM._43, cwRM._44)
                : XMFLOAT4(CW.r[3].m128_f32[0], CW.r[3].m128_f32[1], CW.r[3].m128_f32[2], CW.r[3].m128_f32[3]);
        }

        {
            const XMVECTOR vecZ = XMVectorSet(0.f, 0.f, 1.f, 0.f);
            const XMVECTOR wpW = XMVectorSet(WP.r[0].m128_f32[3], WP.r[1].m128_f32[3],
                WP.r[2].m128_f32[3], WP.r[3].m128_f32[3]);
            const XMVECTOR u30 = XMVectorSubtract(vecZ, wpW);
            XMStoreFloat4(&unk30, u30);
        }

       
        const XMMATRIX CW_noT = drop_translation_col(CW);
        const XMMATRIX PTOW_noProjW = XMMatrixMultiply(CW_noT, PC);
        const XMMATRIX TPTOW_noProjW = XMMatrixMultiply(PTOW_noProjW, TPtP_col);
        store_rm(tptow_no_proj_w, TPTOW_noProjW);

        
        store_rm(camera_to_projective, P);   

    }
    void derive_matrices_ps(const Viewport& viewport)
    {
        resolution_width = viewport.size.x;
        resolution_height = viewport.size.y;

        const XMMATRIX V = load(world_to_camera);
        const XMMATRIX P = load(camera_to_projective);

        constexpr bool kRowMath = true;
        const XMMATRIX VP = kRowMath ? XMMatrixMultiply(V, P) : XMMatrixMultiply(P, V);

        const XMMATRIX CW = XMMatrixInverse(nullptr, V);   
        const XMMATRIX PInv = XMMatrixInverse(nullptr, P); 
        const XMMATRIX VPInv = XMMatrixInverse(nullptr, VP);

        store(camera_to_world, CW);
        store(world_to_projective, VP);
    }
};


struct alignas(16) ScopeViewCB12_VS
{
    DirectX::XMFLOAT4X4 world_to_projective;   
    DirectX::XMFLOAT4X4 camera_to_world;       
    DirectX::XMFLOAT4   target;                
    DirectX::XMFLOAT4   view_miscellaneous;    
    DirectX::XMFLOAT4   view_unk20;            
    DirectX::XMFLOAT4X4 camera_to_projective;  
};


inline void CreateScopeViewCB12(ID3D11Device* dev)
{
    D3D11_BUFFER_DESC bd{};
    bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    bd.ByteWidth = sizeof(ScopeViewCB12_VS);
    bd.Usage = D3D11_USAGE_DYNAMIC;
    bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    ScopeViewCB12_VS zero{}; 
    D3D11_SUBRESOURCE_DATA init{ &zero, 0, 0 };
    dev->CreateBuffer(&bd, &init, g_scopeView_b12.GetAddressOf());
}

inline void UploadScopeViewCB12_All(
    ID3D11DeviceContext* ctx,
    const View& view,
    float targetWidth,
    float targetHeight)
{
    using namespace DirectX;
    D3D11_MAPPED_SUBRESOURCE m{};
    ctx->Map(g_scopeView_b12.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);

    auto* cb = reinterpret_cast<ScopeViewCB12_VS*>(m.pData);
    cb->world_to_projective = view.world_to_projective;
    cb->camera_to_world = view.camera_to_world;
    cb->camera_to_projective = view.camera_to_projective;

    const float invW = targetWidth ? 1.0f / targetWidth : 0.0f;
    const float invH = targetHeight ? 1.0f / targetHeight : 0.0f;
    cb->target = { targetWidth, targetHeight, invW, invH };

    cb->view_miscellaneous = view.view_miscellaneous;               
    cb->view_unk20 = { 4.15325f, 1.24929f, -1.49012e-8f, 1.0f };

    ctx->Unmap(g_scopeView_b12.Get(), 0);

    ID3D11Buffer* b = g_scopeView_b12.Get();
    
    
    
}


struct VSConstants
{
    DirectX::XMFLOAT4 meshOffset_meshScale; 
    DirectX::XMFLOAT4 uvScale_uvOffset;     
};
