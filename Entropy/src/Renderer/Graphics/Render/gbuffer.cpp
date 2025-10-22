#include "Renderer/Graphics/Graphics.h"

// Utility: load compiled HLSL blob and create shader
static bool LoadCSO(const wchar_t* path, Microsoft::WRL::ComPtr<ID3DBlob>& blob) {
    return SUCCEEDED(D3DReadFileToBlob(path, blob.GetAddressOf()));
}

void Graphics::CreateOrResizeGBuffer(UINT w, UINT h)
{
    if (gbuf.w == w && gbuf.h == h && gbuf.albedo) return;
    gbuf = {}; gbuf.w = w; gbuf.h = h;

    auto mkRT = [&](DXGI_FORMAT fmt,
        Microsoft::WRL::ComPtr<ID3D11Texture2D>& tex,
        Microsoft::WRL::ComPtr<ID3D11RenderTargetView>& rtv,
        Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>& srv) {
            D3D11_TEXTURE2D_DESC td{};
            td.Width = w; td.Height = h;
            td.MipLevels = 1; td.ArraySize = 1;
            td.Format = fmt;
            td.SampleDesc = { 1,0 };
            td.Usage = D3D11_USAGE_DEFAULT;
            td.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
            pDevice->CreateTexture2D(&td, nullptr, tex.GetAddressOf());
            pDevice->CreateRenderTargetView(tex.Get(), nullptr, rtv.GetAddressOf());
            pDevice->CreateShaderResourceView(tex.Get(), nullptr, srv.GetAddressOf());
        };

    mkRT(DXGI_FORMAT_R8G8B8A8_UNORM, gbuf.albedo, gbuf.albedoRTV, gbuf.albedoSRV);     // rgb albedo
    mkRT(DXGI_FORMAT_R10G10B10A2_UNORM, gbuf.normalRgh, gbuf.normalRghRTV, gbuf.normalRghSRV);  // xyz normal enc, a rough
    mkRT(DXGI_FORMAT_R8G8B8A8_UNORM, gbuf.material, gbuf.materialRTV, gbuf.materialSRV);   // r metal, g ao

    // Depth that can be sampled
    D3D11_TEXTURE2D_DESC dd{};
    dd.Width = w; dd.Height = h; dd.MipLevels = 1; dd.ArraySize = 1;
    dd.Format = DXGI_FORMAT_R24G8_TYPELESS;
    dd.SampleDesc = { 1,0 };
    dd.Usage = D3D11_USAGE_DEFAULT;
    dd.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    pDevice->CreateTexture2D(&dd, nullptr, gbuf.depth.GetAddressOf());

    D3D11_DEPTH_STENCIL_VIEW_DESC dsvd{};
    dsvd.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
    dsvd.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
    pDevice->CreateDepthStencilView(gbuf.depth.Get(), &dsvd, gbuf.dsv.GetAddressOf());

    D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
    sd.Format = DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
    sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D; sd.Texture2D.MipLevels = 1;
    pDevice->CreateShaderResourceView(gbuf.depth.Get(), &sd, gbuf.depthSRV.GetAddressOf());
}

void Graphics::BindGBufferForWriting()
{
    ID3D11RenderTargetView* rtvs[] = {
        gbuf.albedoRTV.Get(), gbuf.normalRghRTV.Get(), gbuf.materialRTV.Get()
    };
    pContext->OMSetRenderTargets(3, rtvs, gbuf.dsv.Get());
    const float c0[4] = { 0,0,0,0 };
    pContext->ClearRenderTargetView(gbuf.albedoRTV.Get(), c0);
    pContext->ClearRenderTargetView(gbuf.normalRghRTV.Get(), c0);
    pContext->ClearRenderTargetView(gbuf.materialRTV.Get(), c0);
    pContext->ClearDepthStencilView(gbuf.dsv.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
}

void Graphics::RunDeferredLighting()
{
    // bind backbuffer
    pContext->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), nullptr);

    // PS constants: InvProj, CameraPos
    struct CamCB { DirectX::XMFLOAT4X4 InvProj; DirectX::XMFLOAT3 CamPos; float _pad; } cb{};
    XMMATRIX invP = XMMatrixInverse(nullptr, camera.GetProjectionMatrix());
    XMStoreFloat4x4(&cb.InvProj, invP);
    auto cp = camera.GetPositionFloat3();
    cb.CamPos = { cp.x, cp.y, cp.z };

    if (!deferredCamCB) {
        D3D11_BUFFER_DESC bd{};
        bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        bd.ByteWidth = sizeof(CamCB);
        bd.Usage = D3D11_USAGE_DYNAMIC;
        bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        pDevice->CreateBuffer(&bd, nullptr, deferredCamCB.GetAddressOf());
    }
    D3D11_MAPPED_SUBRESOURCE ms{};
    if (SUCCEEDED(pContext->Map(deferredCamCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms))) {
        memcpy(ms.pData, &cb, sizeof(cb));
        pContext->Unmap(deferredCamCB.Get(), 0);
    }
    ID3D11Buffer* b = deferredCamCB.Get();
    pContext->PSSetConstantBuffers(0, 1, &b);

    // bind SRVs
    ID3D11ShaderResourceView* srvs[4] = {
        gbuf.albedoSRV.Get(), gbuf.normalRghSRV.Get(), gbuf.materialSRV.Get(), gbuf.depthSRV.Get()
    };
    pContext->PSSetShaderResources(0, 4, srvs);

    // draw fullscreen triangle
    pContext->IASetInputLayout(nullptr);
    pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    pContext->VSSetShader(fsTriVS.Get(), nullptr, 0);
    pContext->PSSetShader(deferredPS.Get(), nullptr, 0);
    pContext->Draw(3, 0);

    // unbind SRVs to avoid hazards next frame
    ID3D11ShaderResourceView* nulls[4] = {};
    pContext->PSSetShaderResources(0, 4, nulls);
}