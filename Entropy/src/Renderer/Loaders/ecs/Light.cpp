#include "Renderer/Graphics/Graphics.h"


static struct CBLightSimpleGeometry { DirectX::XMFLOAT4X4 transform; };
static Microsoft::WRL::ComPtr<ID3D11Buffer> cbLightSimpleGeometry;

void Graphics::DrawLight(const RenderLight& rl, const View& view)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	wchar_t label[128];
	swprintf(label, 128, L"Light Render %08X %d", rl.parent, rl.idx);
	GpuMarker mark(ctx, label);

	ID3D11RenderTargetView* rts[] = {
		gbufA.light_diffuse.rtv.Get(),
		gbufA.light_specular.rtv.Get()
	};
	ctx->OMSetRenderTargets(2, rts, nullptr);

	using namespace DirectX;

	const XMVECTOR qn = XMQuaternionNormalize(XMVectorSet(rl.rot.w, rl.rot.x, rl.rot.y, rl.rot.z));
	const XMMATRIX R = XMMatrixRotationQuaternion(qn);
	const XMMATRIX T = XMMatrixTranslation(rl.pos.x, rl.pos.y, rl.pos.z);
	const XMMATRIX NodeL2W = R * T;
	const XMMATRIX LightL2W = XMLoadFloat4x4((const XMFLOAT4X4*)&rl.light_matrix);

	const XMMATRIX L2W_scaled = LightL2W * NodeL2W;

	const XMMATRIX W2P = XMLoadFloat4x4(&view.world_to_projective);
	const XMMATRIX SG = L2W_scaled * W2P;

	Mat4 sgM; XMStoreFloat4x4((XMFLOAT4X4*)&sgM, SG);
	externs.SetSimpleGeometryTransform(sgM);


	const XMMATRIX Trel = XMMatrixTranslation(rl.pos.x - view.position.x,
		rl.pos.y - view.position.y,
		rl.pos.z - view.position.z);
	const XMMATRIX L2W_rel_xm = R * Trel;
	const Mat4 l2wRel = M4(L2W_rel_xm);

	Mat4 camT = Mat4::identity();
	{
		float* p = reinterpret_cast<float*>(&camT);
		p[12] = view.position.x; p[13] = view.position.y; p[14] = view.position.z; p[15] = 1.0f;
	}
	externs.SetDeferredLight(camT, l2wRel, rl.unk50);

	ID3D11ShaderResourceView* srvs[] = {
	gbufA.rt1_read.srv.Get(),
	gbufA.depth.texCopySRV.Get(),
	gbufA.rt2.srv.Get(),

	};
	ctx->PSSetShaderResources(0, (UINT)std::size(srvs), srvs);

	if (rl.technique) rl.technique->Bind(pDevice, pContext, externs, states, scopes);

	float bf[4] = { 1,1,1,1 };
	ctx->OMSetBlendState(bsAdditive2RT.Get(), bf, 0xFFFFFFFF);
	ctx->OMSetDepthStencilState(depthStencilLightVolume.Get(), 0);
	ctx->RSSetState(rasterizerStateBiased.Get());

	ID3D11SamplerState* sams[] = { lighting1.Get(), lighting2.Get() };
	ctx->PSSetSamplers(0, (UINT)std::size(sams), sams);


	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	UINT stride = sizeof(float) * 3, offset = 0;
	ID3D11Buffer* vb = lightCubeVB.Get();
	ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
	ctx->IASetIndexBuffer(lightCubeIB.Get(), DXGI_FORMAT_R16_UINT, 0);
	ctx->IASetInputLayout(tiger_input_layouts[0].Get());
	ctx->DrawIndexed(36, 0, 0);
}

void Graphics::CreateLightVolumeResources()
{
    static const float verts[24][3] =
    {
        {  1,  1, -1 }, {  1,  1,  1 }, {  1, -1,  1 }, {  1, -1, -1 },
        { -1, -1, -1 }, { -1, -1,  1 }, { -1,  1,  1 }, { -1,  1, -1 },
        { -1,  1,  1 }, {  1,  1,  1 }, {  1,  1, -1 }, { -1,  1, -1 },
        {  1, -1, -1 }, {  1, -1,  1 }, { -1, -1,  1 }, { -1, -1, -1 },
        {  1, -1,  1 }, {  1,  1,  1 }, { -1,  1,  1 }, { -1, -1,  1 },
        { -1, -1, -1 }, { -1,  1, -1 }, {  1,  1, -1 }, {  1, -1, -1 }
    };

    static const uint16_t idx[36] =
    {
        0,1,2, 2,3,0,
        4,5,6, 6,7,4,
        8,9,10, 10,11,8,
        12,13,14, 14,15,12,
        16,17,18, 18,19,16,
        20,21,22, 22,23,20
    };

    lightCubeIndexCount = (UINT)std::size(idx);

    auto Align16 = [](UINT x) { return (x + 15u) & ~15u; };

    // Vertex buffer
    {
        D3D11_BUFFER_DESC vbd{};
        vbd.ByteWidth = (UINT)sizeof(verts);
        vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
        vbd.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA vinit{};
        vinit.pSysMem = verts;

        HRESULT hr = pDevice->CreateBuffer(&vbd, &vinit, lightCubeVB.GetAddressOf());
        if (FAILED(hr)) { throw std::runtime_error("CreateBuffer(lightCubeVB) failed"); }
    }

    // Index buffer
    {
        D3D11_BUFFER_DESC ibd{};
        ibd.ByteWidth = (UINT)sizeof(idx);
        ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
        ibd.Usage = D3D11_USAGE_IMMUTABLE;

        D3D11_SUBRESOURCE_DATA iinit{};
        iinit.pSysMem = idx;

        HRESULT hr = pDevice->CreateBuffer(&ibd, &iinit, lightCubeIB.GetAddressOf());
        if (FAILED(hr)) { throw std::runtime_error("CreateBuffer(lightCubeIB) failed"); }
    }

    // Constant buffer (must be 16-byte aligned)
    {
        D3D11_BUFFER_DESC cbd{};
        cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbd.ByteWidth = Align16((UINT)sizeof(CBLightSimpleGeometry));
        cbd.Usage = D3D11_USAGE_DYNAMIC;
        cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        CBLightSimpleGeometry zero{};
        D3D11_SUBRESOURCE_DATA cinit{};
        cinit.pSysMem = &zero;

        HRESULT hr = pDevice->CreateBuffer(&cbd, &cinit, cbLightSimpleGeometry.GetAddressOf());
        if (FAILED(hr)) { throw std::runtime_error("CreateBuffer(cbLightSimpleGeometry) failed"); }
    }
}
