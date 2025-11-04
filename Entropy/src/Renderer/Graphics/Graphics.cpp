#include "Graphics.h"
#include <filesystem>
#include <d3dcompiler.h>
#include "Scope/AtmosphereExternUI.h"
#include "Scope/FrameGlobalLightingExternUI.h"
#include "Scope/global_channel_ui.h"
#pragma comment(lib, "d3dcompiler.lib")
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

static inline void BindGBufferForWriting(ID3D11DeviceContext* ctx, const GBufferRT& gbuf)
{
	ID3D11RenderTargetView* rts[3] = {
		gbuf.rt0.rtv.Get(),
		gbuf.rt1.rtv.Get(),
		gbuf.rt2.rtv.Get()
	};
	ctx->OMSetRenderTargets(3, rts, gbuf.depth.dsv.Get()); // ? DSV for depth

	D3D11_TEXTURE2D_DESC d{}; gbuf.rt0.tex->GetDesc(&d);   // ? tex, not texture
	D3D11_VIEWPORT vp{ 0,0, float(d.Width), float(d.Height), 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);
}

#pragma once
#define IDB_ANGlE_LOOKUP 102
static inline void SetFullViewport(ID3D11DeviceContext* ctx, float w, float h)
{
	D3D11_VIEWPORT vp{ 0.0f, 0.0f, w, h, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);
}

static std::array<GlobalChannel, 256> channels = GetGlobalChannelDefaults();

static bool TryGetViewExtern(const ExternStorage& ex, ViewExtern& out)
{
	auto it = ex.scopes.find(TfxExtern::View);
	if (it == ex.scopes.end()) return false;
	const auto& v = it->second.cpu;
	if (v.size() < sizeof(ViewExtern)) return false;
	std::memcpy(&out, v.data(), sizeof(ViewExtern));
	return true;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> cb0_override;

auto ShowMat = [](const char* name, const Mat4& m)
	{
		const float* f = reinterpret_cast<const float*>(&m);
		ImGui::Text("%s", name);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[0], f[1], f[2], f[3]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[4], f[5], f[6], f[7]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[8], f[9], f[10], f[11]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[12], f[13], f[14], f[15]);
	};


// helper – create the 1216-byte PS CB0 (usage: dynamic so we can Map/Unmap like in the capture)
static void EnsureCB0(ID3D11Device* dev, Microsoft::WRL::ComPtr<ID3D11Buffer>& buf)
{
	if (buf) return;
	D3D11_BUFFER_DESC bd{};
	bd.ByteWidth = 76 * 16;                  // 76 float4 = 1216 bytes
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	dev->CreateBuffer(&bd, nullptr, &buf);
}

// exact CB0 contents from your capture (cb0_v0..cb0_v75)
static const float kCB0Data[76][4] = { {0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},{1,1,1,1},
 {0,0,0,0},{1,1,1,1},{0,0,0,0},{0,0,0,0},{0.001f,0,0,0},{0,0,0,0},{1,-1,1,0},{0.25f,0.25f,0.25f,0.25f},
 {1,0,0,0},{0.25f,0.25f,0.25f,0.25f},{-0.5f,0.75f,0,0},{0,0,0,0},{0,0,0,0},{1,1,1,1},
 {0,0,0,0},{0,0,0,0},{1,0,0,0},{0.25f,0.25f,0.25f,0.25f},{1,-1,1,0},{0,0,0,0},{0,0,0,0},{0,0,0,0},
 {1,-1,0,0},{1,1,1,1},{0,0,0,0},{1,1,1,1},{0,100,0,0}
};



void Graphics::Create1x1SRV(UINT color, ComPtr<ID3D11ShaderResourceView>& address)
{
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	td.SampleDesc = { 1,0 };
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA init = { &color, 4, 0 };

	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	pDevice->CreateTexture2D(&td, &init, tex.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
	svd.Format = td.Format;
	svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	svd.Texture2D.MipLevels = 1;

	pDevice->CreateShaderResourceView(tex.Get(), &svd, address.GetAddressOf());
}

struct CBLightSimpleGeometry { DirectX::XMFLOAT4X4 transform; };
Microsoft::WRL::ComPtr<ID3D11Buffer> cbLightSimpleGeometry;


static void DrawFullscreenTriangle(ID3D11DeviceContext* ctx) {
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ctx->IASetInputLayout(nullptr);   // shader must be VS that generates a triangle or uses no inputs
	ctx->Draw(4, 0);
}

static DirectX::XMMATRIX MakeReversedZProjLH(float fovY, float aspect, float zNear)
{
	// Infinite far plane, left-handed, reversed-Z
	const float y = 1.0f / std::tan(fovY * 0.5f);
	const float x = y / aspect;

	using namespace DirectX;
	XMMATRIX M;
	M.r[0] = XMVectorSet(x, 0, 0, 0);
	M.r[1] = XMVectorSet(0, y, 0, 0);
	M.r[2] = XMVectorSet(0, 0, 0, 1);    // puts depth in w
	M.r[3] = XMVectorSet(0, 0, zNear, 0);
	return M;
}

struct GpuMarker {
	ID3DUserDefinedAnnotation* a{};
	GpuMarker(ID3D11DeviceContext* ctx, const wchar_t* name) {
		if (SUCCEEDED(ctx->QueryInterface(IID_PPV_ARGS(&a))) && a) a->BeginEvent(name);
	}
	~GpuMarker() { if (a) { a->EndEvent(); a->Release(); } }
};

Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> anno_;
void Graphics::InitAnnotation() {
	pContext->QueryInterface(IID_PPV_ARGS(anno_.GetAddressOf())); // may be null
}

// RAII scope marker
struct ScopedGpuEvent {
	ID3DUserDefinedAnnotation* a{};
	ScopedGpuEvent(ID3DUserDefinedAnnotation* anno, const wchar_t* name) : a(anno) {
		if (a) a->BeginEvent(name);
	}
	template<typename... Args>
	ScopedGpuEvent(ID3DUserDefinedAnnotation* anno, const wchar_t* fmt, Args... args) : a(anno) {
		if (!a) return;
		wchar_t buf[256];
		_snwprintf_s(buf, _TRUNCATE, fmt, args...);
		a->BeginEvent(buf);
	}
	~ScopedGpuEvent() { if (a) a->EndEvent(); }
};



void CreateCB1(ID3D11Device* dev) {
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(CB1Payload);
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	CB1Payload zero{};
	D3D11_SUBRESOURCE_DATA init{ &zero, 0, 0 };
	dev->CreateBuffer(&bd, &init, g_cb1.GetAddressOf());
}

void Graphics::InitializeInputLayouts()
{

	for (size_t i = 0; i < INPUT_LAYOUTS.size(); ++i) {
		Microsoft::WRL::ComPtr<ID3D11InputLayout> il;
		HRESULT hr = CreateInputLayoutFromTigerLayout(pDevice.Get(), INPUT_LAYOUTS[i], il);
		if (FAILED(hr) || !il) {
			char msg[256];
			sprintf_s(msg, "Tiger IL[%zu] creation failed (hr=0x%08X)\n", i, (unsigned)hr);
			OutputDebugStringA(msg);
		}
		else {
			tiger_input_layouts[i] = std::move(il);
		}
	}
}

bool Graphics::InitializeRenderGlobals()
{
	auto& renderGlobalTechnique = GlobalData::getGlobalTechniques();
	for (auto& global : renderGlobalTechnique)
	{
		this->globalTechniques.emplace(global.first, this->assets->EnqueueTechnique(global.second));
	}
	return true;
}

inline float float_from_bits(std::uint32_t bits) {
	return std::bit_cast<float>(bits);
}
Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> anno;

void Graphics::DrawStaticMesh(const RenderStatic& rs, const View& view)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	wchar_t label[128];
	swprintf(label, 128, L"Static geometry Opaque %08X", rs.mesh->id);
	//const DirectX::XMMATRIX world = rs.world;
	//printf("Loading Static %08X", rs.mesh->id);
	GpuMarker mark(ctx, label);
	const auto& mesh = *rs.mesh;

	// View constant buffer(s)
	ID3D11Buffer* b1 = g_cb1.Get();            // if you also use cb1 for per-object, leave it bound
	
	ID3D11Buffer* b12 = g_scopeView_b12.Get();
	XMFLOAT3 mesh_offset;
	mesh_offset.x = rs.meshData.mesh_offset[0];
	mesh_offset.y = rs.meshData.mesh_offset[1];
	mesh_offset.z = rs.meshData.mesh_offset[2];

	UpdateCB1(ctx,
		mesh_offset,
		rs.meshData.mesh_scale,
		rs.meshData.texture_coordinate_scale,
		rs.meshData.texture_coordinate_offset[0],
		rs.meshData.texture_coordinate_offset[1],
		rs.meshData.max_colour_index,
		rs.world);
	for (const auto& part : mesh.parts)
	{
		auto tech = part.technique.get();
		ctx->IASetInputLayout(tiger_input_layouts[mesh.input_layout_index].Get());

		// --- Buffers (choose the group this part uses; here we take the first)
		const size_t gi = part.bufferGroupIndices.empty() ? 0 : part.bufferGroupIndices[0];
		if (gi >= mesh.groups.size() || !mesh.groups[gi]) {
			printf("Skip part: bad buffer group index\n");
			continue;
		}
		const BufferGroup& bg = *mesh.groups[gi];
		if (!bg.index || !bg.vertex || bg.indexCount == 0) {
			//printf("Skip part: missing VB/IB or zero indexCount\n");
			continue;
		}


		ID3D11Buffer* vbs[3];
		UINT          strides[3];
		UINT          offsets[3] = { 0,0,0 };
		UINT          vbCount = 0;

		if (bg.vertex) { vbs[vbCount] = bg.vertex.get(); strides[vbCount] = bg.vertexStride; ++vbCount; }
		if (bg.uv) { vbs[vbCount] = bg.uv.get();     strides[vbCount] = bg.uvStride;     ++vbCount; }
		//if (bg.color) { vbs[vbCount] = bg.color.get();  strides[vbCount] = bg.colorStride;  ++vbCount; }

		ctx->IASetVertexBuffers(0, vbCount, vbs, strides, offsets);
		ctx->IASetIndexBuffer(bg.index.get(), bg.indexFormat, 0);

	
		ID3D11ShaderResourceView* s = bg.color.get();
		//ctx->VSSetShaderResources(1, 1, &s);
		ctx->VSSetShaderResources(0, 1, &s);
		pContext->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0);
		if (tech)
			tech->Bind(pDevice, pContext, externs, states, scopes);

		
		ctx->VSSetConstantBuffers(1, 1, &b1);
		const UINT instanceCount = (UINT)rs.world.size();
		ctx->DrawIndexedInstanced(part.partInfo.index_count,
			instanceCount,               // InstanceCount
			part.partInfo.index_start,   // StartIndexLocation
			0,                           // BaseVertexLocation
			0);
	}

}

void Graphics::DrawStaticSpecial(const RenderStatic& rs, const View& view)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	wchar_t label[128];
	swprintf(label, 128, L"Static geometry Special %08X", rs.mesh->id);
	//const DirectX::XMMATRIX world = rs.world;
	//printf("Loading Static %08X", rs.mesh->id);
	GpuMarker mark(ctx, label);
	const auto& mesh = *rs.mesh;

	// View constant buffer(s)
	ID3D11Buffer* b1 = g_cb1.Get();            // if you also use cb1 for per-object, leave it bound

	ID3D11Buffer* b12 = g_scopeView_b12.Get();
	XMFLOAT3 mesh_offset;
	mesh_offset.x = rs.meshData.mesh_offset[0];
	mesh_offset.y = rs.meshData.mesh_offset[1];
	mesh_offset.z = rs.meshData.mesh_offset[2];

	UpdateCB1(ctx,
		mesh_offset,
		rs.meshData.mesh_scale,
		rs.meshData.texture_coordinate_scale,
		rs.meshData.texture_coordinate_offset[0],
		rs.meshData.texture_coordinate_offset[1],
		rs.meshData.max_colour_index,
		rs.world);
	swprintf(label, 128, L"Static geometry decal %08X", rs.mesh->id);
	for (const auto& special : rs.specials) {
		// If technique is shared_ptr:
		auto tech = special->technique;
		if (special->part.TfxRenderStage == 1) {
			auto& part = special->part;
			auto tech = special->technique.get();
			ctx->IASetInputLayout(tiger_input_layouts[mesh.input_layout_index].Get());
			ID3D11Buffer* vbs[3];
			UINT          strides[3];
			UINT          offsets[3] = { 0,0,0 };
			UINT          vbCount = 0;
			if (special->group->vertex) { vbs[vbCount] = special->group->vertex.get(); strides[vbCount] = special->group->vertexStride; ++vbCount; }
			if (special->group->uv) { vbs[vbCount] = special->group->uv.get();     strides[vbCount] = special->group->uvStride;     ++vbCount; }

			ctx->IASetVertexBuffers(0, vbCount, vbs, strides, offsets);
			ctx->IASetIndexBuffer(special->group->index.get(), special->group->indexFormat, 0);

			tech->Bind(pDevice, pContext, externs, states, scopes);
			/*ID3D11ShaderResourceView* rt1decal[1] = {
			gbufA.rt1_read.srv.Get() 
			};
			pContext->PSSetShaderResources(2, 1, rt1decal);*/
			pContext->OMSetDepthStencilState(depthStencilDecal.Get(),0);
			ID3D11Buffer* b = g_cb1.Get();
			ctx->VSSetConstantBuffers(1, 1, &b);
			const UINT instanceCount = (UINT)rs.world.size();
			ctx->DrawIndexedInstanced(part.index_count,
				instanceCount,               // InstanceCount
				part.index_start,   // StartIndexLocation
				0,                           // BaseVertexLocation
				0);

			
			
		}//decals
	}
}

void Graphics::DrawStaticTransparent(const RenderStatic& rs, const View& view)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	wchar_t label[128];
	swprintf(label, 128, L"Static geometry transparent %08X", rs.mesh->id);
	//const DirectX::XMMATRIX world = rs.world;
	//printf("Loading Static %08X", rs.mesh->id);
	GpuMarker mark(ctx, label);
	const auto& mesh = *rs.mesh;

	// View constant buffer(s)
	ID3D11Buffer* b1 = g_cb1.Get();            // if you also use cb1 for per-object, leave it bound

	ID3D11Buffer* b12 = g_scopeView_b12.Get();
	XMFLOAT3 mesh_offset;
	mesh_offset.x = rs.meshData.mesh_offset[0];
	mesh_offset.y = rs.meshData.mesh_offset[1];
	mesh_offset.z = rs.meshData.mesh_offset[2];

	UpdateCB1(ctx,
		mesh_offset,
		rs.meshData.mesh_scale,
		rs.meshData.texture_coordinate_scale,
		rs.meshData.texture_coordinate_offset[0],
		rs.meshData.texture_coordinate_offset[1],
		rs.meshData.max_colour_index,
		rs.world);
	swprintf(label, 128, L"Static geometry decal %08X", rs.mesh->id);
	for (const auto& special : rs.specials) {
		// If technique is shared_ptr:
		auto tech = special->technique;
		if (special->part.TfxRenderStage == 7) {
			auto& part = special->part;
			auto tech = special->technique.get();
			ctx->IASetInputLayout(tiger_input_layouts[part.input_layout_index].Get());
			if (part.PrimitiveType == 5) {
				pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			}
			else {
				pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			}
			//printf("%d\n", part.input_layout_index);
			ID3D11Buffer* vbs[3];
			UINT          strides[3];
			UINT          offsets[3] = { 0,0,0 };
			UINT          vbCount = 0;
			
			

			if (special->group->vertex) { vbs[vbCount] = special->group->vertex.get(); strides[vbCount] = special->group->vertexStride; ++vbCount; }
			if (special->group->uv) { vbs[vbCount] = special->group->uv.get();     strides[vbCount] = special->group->uvStride;     ++vbCount; }
			if (special->part.VertexColourBuffer.hash != 0xffffffff) {
				ID3D11ShaderResourceView* s = special->group->color.get();
				//ctx->VSSetShaderResources(1, 1, &s);
				ctx->VSSetShaderResources(0, 1, &s);
			}
			ctx->IASetVertexBuffers(0, vbCount, vbs, strides, offsets);
			ctx->IASetIndexBuffer(special->group->index.get(), special->group->indexFormat, 0);
			if (tech)
				tech->Bind(pDevice, pContext, externs, states, scopes);
			pContext->OMSetDepthStencilState(depthStencilDecal.Get(), 0);
			pContext->RSSetState(rasterizerStateNoCull.Get());
			ID3D11ShaderResourceView* srvs[] = {            // t4
			this->temp_angle_lookup.Get(),             // t5
			};
			pContext->PSSetShaderResources(15, 1, srvs);
			ID3D11Buffer* b = g_cb1.Get();
			ctx->VSSetConstantBuffers(1, 1, &b);
			
			const UINT instanceCount = (UINT)rs.world.size();
			ctx->DrawIndexedInstanced(part.index_count,
				instanceCount,               // InstanceCount
				part.index_start,   // StartIndexLocation
				0,                           // BaseVertexLocation
				0);
			SetFullViewport(ctx, float(windowWidth), float(windowHeight));



		}//decals
	}
}

void Graphics::DrawLight(const RenderLight& rl, const View& view)
{
    ID3D11DeviceContext* ctx = pContext.Get();
    wchar_t label[128];
    swprintf(label, 128, L"Light Render %08X %d", rl.parent, rl.idx);
    GpuMarker mark(ctx, label);
	//printf("Light Render %08X %d  at pos x %f %f %f\n", rl.parent, rl.idx, rl.pos.x, rl.pos.y, rl.pos.z);
    // ------------------- Bind light MRTs + scene depth -------------------
    ID3D11RenderTargetView* rts[] = {
        gbufA.light_diffuse.rtv.Get(),
        gbufA.light_specular.rtv.Get()
    };
    ctx->OMSetRenderTargets(2, rts, gbufA.depth.dsv.Get());

    using namespace DirectX;

    // Node transform (local_to_world)
    const XMVECTOR qn = XMQuaternionNormalize(XMVectorSet(rl.rot.x, rl.rot.y, rl.rot.z, rl.rot.w));
    const XMMATRIX R  = XMMatrixRotationQuaternion(qn);
    const XMMATRIX T  = XMMatrixTranslation(rl.pos.x, rl.pos.y, rl.pos.z);
	const XMMATRIX NodeL2W = R * T;
	const XMMATRIX LightL2W = XMLoadFloat4x4((const XMFLOAT4X4*)&rl.light_matrix);

	// Rust:   Node * Light   (column-vectors)  =>  apply Light then Node
	// C++:    Light * Node   (row-vectors)     =>  same effect
	const XMMATRIX L2W_scaled = LightL2W * NodeL2W;

	// final SG = (world_to_projective * transform_mat_scaled) in Rust
	// For row-vectors, multiply on the right:
	const XMMATRIX W2P = XMLoadFloat4x4(&view.world_to_projective);
	const XMMATRIX SG = L2W_scaled * W2P;

	Mat4 sgM; XMStoreFloat4x4((XMFLOAT4X4*)&sgM, SG);
	externs.SetSimpleGeometryTransform(sgM);


	const XMMATRIX Trel = XMMatrixTranslation(rl.pos.x - view.position.x,
		rl.pos.y - view.position.y,
		rl.pos.z - view.position.z);
	const XMMATRIX L2W_rel_xm = R * Trel;

	// convert XMMATRIX -> Mat4
	const Mat4 l2wRel = M4(L2W_rel_xm);

	// camT as Mat4 (you can also make it via M4(XMMatrixTranslation(...)))
	Mat4 camT = Mat4::identity();
	{
		float* p = reinterpret_cast<float*>(&camT);
		p[12] = view.position.x; p[13] = view.position.y; p[14] = view.position.z; p[15] = 1.0f;
	}

	// set externs
	externs.SetDeferredLight(camT, l2wRel, rl.unk50);

    // ------------------- SRVs (order matters!) -------------------
	ID3D11ShaderResourceView* srvs[] = {
	gbufA.rt1_read.srv.Get(), 
	gbufA.depth.texCopySRV.Get(),// t0 : MATERIAL/PARAMS  (RT2)
	gbufA.rt2.srv.Get(),     // t1 : NORMAL+ROUGHNESS (RT1 clone)
	 // t2 : DEPTH            (R32_FLOAT)
	// (t3, t4, t5 can be bound by the technique if used)
	};
	ctx->PSSetShaderResources(0, (UINT)std::size(srvs), srvs);

	// ------------------- States for light volumes -------------------
	if (rl.technique) rl.technique->Bind(pDevice, pContext, externs, states, scopes);

	float bf[4] = { 1,1,1,1 };
	ctx->OMSetBlendState(bsAdditive2RT.Get(), bf, 0xFFFFFFFF);
	ctx->OMSetDepthStencilState(depthStencilLightVolume.Get(), 0);
	ctx->RSSetState(rasterizerStateBiased.Get());

	// Samplers: s0 = point-clamp (GBuffer), s1 = linear (gathers/LUTs)
	ID3D11SamplerState* sams[] = { lighting1.Get(), lighting2.Get() };
	ctx->PSSetSamplers(0, (UINT)std::size(sams), sams);



    // ------------------- Draw the light volume -------------------
    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    UINT stride = sizeof(float) * 3, offset = 0;
    ID3D11Buffer* vb = lightCubeVB.Get();
    ctx->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
    ctx->IASetIndexBuffer(lightCubeIB.Get(), DXGI_FORMAT_R16_UINT, 0);
    ctx->IASetInputLayout(tiger_input_layouts[0].Get());
    ctx->DrawIndexed(36, 0, 0);

    // Unbind to avoid hazards
    ID3D11ShaderResourceView* nulls[3] = { nullptr, nullptr, nullptr };
    ctx->PSSetShaderResources(0, 3, nulls);
}

void Graphics::DrawEntity(const RenderEntity& rs, const View& /*view*/, TfxRenderStage renderStage)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	wchar_t label[128];
	swprintf(label, 128, L"Dynamic Object %08X ", rs.id);
	GpuMarker mark(ctx, label);
	// Build a single world matrix from the entity's pos/rot (glm -> XM)
	using namespace DirectX;
	const XMVECTOR q = XMVectorSet(rs.rot.x, rs.rot.y, rs.rot.z, rs.rot.w);
	const XMVECTOR qn = XMQuaternionNormalize(q);
	const XMMATRIX R = XMMatrixRotationQuaternion(qn);
	const XMMATRIX T = XMMatrixTranslation(rs.pos.x, rs.pos.y, rs.pos.z);
	XMFLOAT4X4 worldXM;
	XMStoreFloat4x4(&worldXM, R * T);

	// CB1: mesh/model params from SEntityModel (fallbacks are sane if authoring data is odd)
	XMFLOAT3 mesh_offset{
		rs.meshData.model_offset.x,
		rs.meshData.model_offset.y,
		rs.meshData.model_offset.z
	};
	const float mesh_scale = rs.meshData.model_scale.x;                  // uniform; use .x
	const float texScale = rs.meshData.texcoord_scale.x;               // if non-uniform you can split
	const float texOffX = rs.meshData.texcoord_offset.x;
	const float texOffY = rs.meshData.texcoord_offset.y;
	const uint32_t maxColourIndex = 0;                                   // no field on SEntityModel; keep 0

	{
		UpdateCB1_Single(ctx,
			rs.meshData.model_offset,
			mesh_scale,
			texScale,
			texOffX,
			texOffY,
			rs.rot,
			rs.pos);
	}
	// Draw each mesh and its parts
	for (const auto& meshPtr : rs.meshs)
	{

		if (!meshPtr) continue;
		const DynamicMesh& mesh = *meshPtr;

		// Pick input layout for the current pipeline stage (GBuffer)
		const uint8_t ilIndex = mesh.input_layout_per_render_stage[static_cast<int>(TfxRenderStage::GenerateGbuffer)];
		if (ilIndex < tiger_input_layouts.size() && tiger_input_layouts[ilIndex])
			ctx->IASetInputLayout(tiger_input_layouts[ilIndex].Get());
		else
			ctx->IASetInputLayout(nullptr);

		// Buffers group
		const auto& grp = mesh.buffers;
		if (!grp || !grp->index_buffer) continue;

		// Set VBs (slot0 = vertex0 / slot1 = vertex1)
		ID3D11Buffer* vbs[4]{};
		UINT          strides[4]{};
		UINT          offsets[4]{ 0,0,0,0 };
		UINT          vbCount = 0;

		if (grp->vertex0_buffer) { vbs[vbCount] = grp->vertex0_buffer.get(); strides[vbCount] = grp->vertex0Stride; ++vbCount; }
		if (grp->vertex1_buffer) { vbs[vbCount] = grp->vertex1_buffer.get(); strides[vbCount] = grp->vertex1Stride; ++vbCount; }
		if (grp->buffer2) { vbs[vbCount] = grp->buffer2.get();        strides[vbCount] = grp->buffer2Stride; ++vbCount; }
		if (grp->buffer3) { vbs[vbCount] = grp->buffer3.get();        strides[vbCount] = grp->buffer3Stride; ++vbCount; }

		ctx->IASetVertexBuffers(0, vbCount, vbs, strides, offsets);
		ctx->IASetIndexBuffer(grp->index_buffer.get(), grp->indexFormat, 0);

		// Optional vertex colour SRV (matches how you handled statics)
		if (grp->color) {
			ID3D11ShaderResourceView* s = grp->color.get();
			ID3D11Buffer* b = g_cb1.Get();
			ctx->VSSetShaderResources(0, 1, &s);
			ctx->VSSetConstantBuffers(1, 1, &b);
		}

		// Draw each part with its technique
		auto start = mesh.part_range_per_render_stage[static_cast<int>(renderStage)];
		auto end = mesh.part_range_per_render_stage[static_cast<int>(renderStage)+1];
		for (size_t i = start; i < end; i += 1)
		{
			const auto& partPtr = mesh.parts[i];
			//if (!partPtr) continue;

			const DynamicMeshPart& part = *partPtr;
			if (part.meshpartinfo.LodCatagory > 2) continue;

			// Primitive type
			if (part.meshpartinfo.PrimitiveType == 5)
				ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			else
				ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			// Set constant buffer
			ID3D11Buffer* b1 = g_cb1.Get();
			ctx->VSSetConstantBuffers(1, 1, &b1);
			// Technique binding
			//ctx->PSSetConstantBuffers(1, 1, &b1);
			if (part.meshpartinfo.varient_shader_index == 0xFFFF) {
				part.technique->Bind(pDevice, pContext, externs, states, scopes);
			} 
			else {
				auto tech = rs.external_mats[rs.external_material_mapping[part.meshpartinfo.varient_shader_index].technique_start];
				if (tech)
					tech->Bind(pDevice, pContext, externs, states, scopes);
			}

			
			

			// Optional skinning VS override
			if (mesh.buffers->skinning_buffer.get())
				pContext->VSSetShader(entity_vs_override.Get(), nullptr, 0);
			ctx->VSSetConstantBuffers(1, 1, &b1);
			
			// Draw call
			ctx->DrawIndexed(
				part.meshpartinfo.index_count,
				part.meshpartinfo.index_start,
				0
			);
		}
	}
}

void Graphics::RenderFrame()
{
	mainQueue->Drain();
	gTimer.tick();
	// Per-frame externs
	externs.SetFrameTimes(float(gTimer.total_game_time()), float(gTimer.delta_game_time()), 4.0f);
	externs.SetFxaa(float(gTimer.total_game_time()));

	// Build view/projection (reversed-Z projection)
	View viewState{};
	XMStoreFloat4x4(&viewState.world_to_camera, camera.GetViewMatrix());
	const float fovY = DirectX::XMConvertToRadians(120.0f);
	const float aspect = float(windowWidth) / float(windowHeight);
	DirectX::XMMATRIX projRZ = MakeReversedZProjLH(fovY, aspect, camera.GetNearZ());
	XMStoreFloat4x4(&viewState.camera_to_projective, projRZ);
	viewState.derive_matrices_vs({ { float(windowWidth), float(windowHeight) } });

	{
		// View (needs to exist before you draw statics into the GBuffer)
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;

		externs.SetViewProjectiveToCamera(viewState, vp);

		// Make sure CBs exist and push current bytes so the GBuffer pass can use them
		externs.EnsureAll(pDevice.Get());
		externs.UploadAll(pContext.Get());
	}

	for (auto& scope : scopes)
		scope.second.UpdateScopeBuffers(pContext, externs);

	// =========================
	// generate_gbuffer
	// =========================


	{
		ScopedGpuEvent e(anno_.Get(), L"generate_gbuffer");

		ID3D11RenderTargetView* rt_gbuf[3]{
			gbufA.rt0.rtv.Get(),
				gbufA.rt1.rtv.Get(),
				gbufA.rt2.rtv.Get()
		};

		pContext->OMSetRenderTargets(3, rt_gbuf, gbufA.depth.dsv.Get());
		const float black[4] = { 0,0,0,0 };
		const float black2[4] = { 1,0.5,1,1 };
		D3D11_VIEWPORT vp{ 0,0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);
		pContext->ClearRenderTargetView(gbufA.rt0.rtv.Get(), black);
		pContext->ClearRenderTargetView(gbufA.rt1.rtv.Get(), black);
		pContext->ClearRenderTargetView(gbufA.rt2.rtv.Get(), black2);

		//gbufA.BindGBufferForWriting(pContext.Get());
		pContext->ClearDepthStencilView(
			gbufA.depth.dsv.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			0.0f,   // <<<<<< IMPORTANT for reversed-Z
			0
		);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(states.blend_states[0].Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(states.depth_stencil_states[2].first.Get(), 0);
		pContext->RSSetState(states.rasterizer_states[2].Get());
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pipelineStage = TfxRenderStage::GenerateGbuffer;

		for (auto& rs : staticsToDraw) DrawStaticMesh(rs, viewState);
		for (auto& re : entitiesToDraw) DrawEntity(re, viewState);
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (RT1->RT1_Clone)");
		pContext->CopyResource(gbufA.rt1_read.tex.Get(), gbufA.rt1.tex.Get());
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (Depth->DepthCopySRV)");
		if (gbufA.depth.texCopy && gbufA.depth.tex)
			pContext->CopyResource(gbufA.depth.texCopy.Get(), gbufA.depth.tex.Get());
	}
	{   // decals
		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
		for (auto& rs : staticsToDraw)
			DrawStaticSpecial(rs, viewState);
	}

	//const float clearRt0[4] = { 0, 0, 0, 1 };
	//pContext->ClearRenderTargetView(gbufA.rt0.rtv.Get(), clearRt0);

	//// RT1: Normal.xy in 0..1, Normal.z=1, Roughness=1
	//const float clearRt1[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
	//pContext->ClearRenderTargetView(gbufA.rt1.rtv.Get(), clearRt1);

	//// RT2: Material params – keep them “no spec”
	//const float clearRt2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
	//pContext->ClearRenderTargetView(gbufA.rt2.rtv.Get(), clearRt2);
	// =========================
	// lighting_pass
	// =========================
	{
		ScopedGpuEvent e(anno_.Get(), L"lighting_pass");

		ID3D11RenderTargetView* rts[2] = {
			gbufA.light_diffuse.rtv.Get(),
			gbufA.light_specular.rtv.Get(),
		};
		pContext->OMSetRenderTargets(2, rts, nullptr);

		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));

		const float black[4] = { 0,0,0,0 };
		const float dim[4] = { 0.001f,0.001f,0.001f,0.001f };
		pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), dim);
		pContext->ClearRenderTargetView(gbufA.light_specular.rtv.Get(), black);
		pContext->ClearRenderTargetView(gbufA.light_ibl_specular.rtv.Get(), black);

		if (auto it = globalTechniques.find("global_lighting"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		pContext->OMSetDepthStencilState(depthStencilStateLighting.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());


		ID3D11ShaderResourceView* srvs[] = {
			gbufA.rt2.srv.Get(),          // t0 material/params
			gbufA.rt1_read.srv.Get(),     // t1 normal+roughness
			gbufA.depth.texCopySRV.Get(), // t2 depth
			white1x1SRV.Get(),            // t3
			white1x1SRV.Get(),            // t4
			white1x1SRV.Get(),            // t5
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs), srvs);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);

		ID3D11SamplerState* sams2[] = { lighting1.Get(), lighting2.Get() };
		pContext->PSSetSamplers(0, (UINT)std::size(sams2), sams2);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		for (auto& light : lightsToDraw)
			DrawLight(light, viewState);

		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
	}

	// =========================
	// deferred_shading
	// =========================
	{
		ScopedGpuEvent e(anno_.Get(), L"deferred_shading");

		ID3D11RenderTargetView* rt = gbufA.shading_result.rtv.Get();
		pContext->OMSetRenderTargets(1, &rt, nullptr);

		if (auto it = globalTechniques.find("deferred_shading_no_atm"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		D3D11_VIEWPORT vp{ 0,0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);

		ID3D11ShaderResourceView* srvs2[] = {
			gbufA.rt1_read.srv.Get(),          // t0 : normals
			gbufA.rt0.srv.Get(),               // t1 : albedo
			gbufA.rt2.srv.Get(),               // t2 : material
			gbufA.light_diffuse.srv.Get(),     // t3
			gbufA.light_specular.srv.Get(),    // t4
			gbufA.light_ibl_specular.srv.Get(),// t5
			this->global_textures.find("iridescence_lookup_texture")->second.Get(), // t6
			white1x1SRV.Get(),                 // t7
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs2), srvs2);

		ID3D11SamplerState* samsShade[] = { shading1.Get(), shading2.Get() };
		pContext->PSSetSamplers(0, (UINT)std::size(samsShade), samsShade);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
	}

	// =========================
	// transparency pass
	// =========================
	{
		ID3D11ShaderResourceView* nulls[16] = {};
		pContext->PSSetShaderResources(0, 16, nulls);
		pContext->VSSetShaderResources(0, 16, nulls);

		ID3D11RenderTargetView* rt[1] = { gbufA.shading_result.rtv.Get() };
		pContext->OMSetRenderTargets(1, rt, gbufA.depth.dsv.Get());

		ScopedGpuEvent e(anno_.Get(), L"transparency_pass");
		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetDepthStencilState(depthStencilDecal.Get(), 0); // reversed-Z read-only
		pContext->RSSetState(rasterizerStateNoCull.Get());
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		for (auto& rs : staticsToDraw)   DrawStaticTransparent(rs, viewState);
		for (auto& rs : entitiesToDraw)  DrawEntity(rs, viewState, TfxRenderStage::Transparents);

		pContext->PSSetShaderResources(0, 16, nulls);
		pContext->VSSetShaderResources(0, 16, nulls);
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
	}

	// =========================
	// present_to_backbuffer
	// =========================
	{
		ScopedGpuEvent e(anno_.Get(), L"present_to_backbuffer");

		// sRGB RTV for the *final combine* (normal present path)
		ID3D11RenderTargetView* bbSRGB = pRenderTargetView.Get();
		pContext->OMSetRenderTargets(1, &bbSRGB, nullptr);

		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &vp);

		const float clear[4] = { 0,0,0,1 };
		pContext->ClearRenderTargetView(bbSRGB, clear);

		if (auto it = globalTechniques.find("final_combine"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		ID3D11ShaderResourceView* src = gbufA.shading_result.srv.Get();
		pContext->PSSetShaderResources(0, 1, &src);
		ID3D11SamplerState* samp = samplerLinearClamp.Get();
		pContext->PSSetSamplers(0, 1, &samp);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		pContext->PSSetShaderResources(0, 1, nullSRV);
	}

	// =========================
	// DEBUG PREVIEW (linear RTV like Alkahest)
	// =========================
	static bool drawrt1 = false, drawrt0 = false, drawrt2 = false, drawLight_diffuse = false,
		drawLight_specular = false, drawDepth = false, drawShading = false,
		drawShadingRead = false, drawLight_ibl = false;

	const bool wantDebugPreview =
		drawrt0 || drawrt1 || drawrt2 || drawLight_diffuse || drawLight_specular ||
		drawLight_ibl || drawShading || drawShadingRead;

	// If any preview is on, switch to the **linear UNORM RTV** so we don't post-gamma the floats.
	if (wantDebugPreview)
	{
		ID3D11RenderTargetView* bbLinear = pRenderTargetViewLinear.Get(); // created at init
		pContext->OMSetRenderTargets(1, &bbLinear, nullptr);
		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));
	}

	if (!fpsTimer.isrunning) { fpsTimer.Start(); } static int fpsCounter = 0; static std::string fpsString = "FPS: 0"; if (++fpsCounter, fpsTimer.GetMilisecondsElapsed() > 1000) { fpsString = "FPS: " + std::to_string(fpsCounter); fpsCounter = 0; fpsTimer.Restart(); }

	auto CameraPos = camera.GetPositionFloat3(); std::string CameraPrint = std::format("X: {:.2f} Y: {:.2f} Z: {:.2f}", CameraPos.x, CameraPos.y, CameraPos.z); auto CameraRot = camera.GetRotationFloat3(); std::string CameraPrintRot = std::format("Pitch: {:.2f} Roll: {:.2f} Yaw: {:.2f}", CameraRot.x, CameraRot.y, CameraRot.z);

	// SpriteBatch preview draws
	spriteBatch->Begin();
	if (drawrt1)           spriteBatch->Draw(gbufA.rt1_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt0)           spriteBatch->Draw(gbufA.rt0.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt2)           spriteBatch->Draw(gbufA.rt2.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_diffuse) spriteBatch->Draw(gbufA.light_diffuse.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_specular)spriteBatch->Draw(gbufA.light_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_ibl)     spriteBatch->Draw(gbufA.light_ibl_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawShading)       spriteBatch->Draw(gbufA.shading_result.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawShadingRead)   spriteBatch->Draw(gbufA.shading_result_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(), DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(), DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrintRot).c_str(), DirectX::XMFLOAT2(0, 100), DirectX::Colors::Wheat);
	spriteBatch->End();

	// UI (render to whichever RTV is currently bound; linear if previewing)
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Debug Menu");
	static float value = 50.0f;
	if (ImGui::DragFloat("Speed X:", &value, 1, 0.0f, 100.0f, "%.0f%%"))
		camera.SetSpeed(value / 10.0f);
	ImGui::Checkbox("Draw gbuffer", &drawrt0);
	ImGui::Checkbox("Draw Rt1", &drawrt1);
	ImGui::Checkbox("Draw Rt2", &drawrt2);
	ImGui::Checkbox("Draw Light_diffuse", &drawLight_diffuse);
	ImGui::Checkbox("Draw Light_specular", &drawLight_specular);
	ImGui::Checkbox("Draw Depth", &drawDepth);
	ImGui::Checkbox("Draw Shading", &drawShading);
	ImGui::Checkbox("Draw drawLight_ibl", &drawLight_ibl);
	ImGui::Checkbox("Draw ShadingRead", &drawShadingRead);
	// ... (rest of your ImGui panes unchanged)
	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	pSwapChain->Present(1, 0);
}


bool Graphics::Initialize(HWND hWnd, int width, int height)
{
	this->windowWidth = width;
	this->windowHeight = height;

	if (!InitializeDirectX(hWnd))
		return false;
	OutputDebugStringA("DirectX initialized.\n");
	if (!InitializeShaders())
		return false;
	OutputDebugStringA("Shaders initialized.\n");
	if (!InitializeRenderGlobals())
		return false;
	if (!InitializeScene())
		return false;
	OutputDebugStringA("Scene initialized.\n");

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX11_Init(this->pDevice.Get(), this->pContext.Get());
	ImGui::StyleColorsDark();


	return true;
}

bool Graphics::InitializeDirectX(HWND hWnd)
{
	try
	{
		std::vector<GPUAdapter> adapters = GPUReader::GetAdapterData();

		if (adapters.size() == 0)
		{
			ErrorLogger::Log("No GPU adapters found!");
			return false;
		}
		UINT createDeviceFlags = 0;
#if defined(_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;  // Enable debug layer
#endif
		DXGI_SWAP_CHAIN_DESC scd = { 0 };
		scd.BufferDesc.Width = this->windowWidth;
		scd.BufferDesc.Height = this->windowHeight;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;

		scd.SampleDesc.Count = 1;
		scd.SampleDesc.Quality = 0;

		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.BufferCount = 1;
		scd.OutputWindow = hWnd;
		scd.Windowed = TRUE;
		scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		HRESULT hr;
		hr = D3D11CreateDeviceAndSwapChain(
			adapters[0].pAdapter,
			D3D_DRIVER_TYPE_UNKNOWN,
			NULL,
			createDeviceFlags,
			NULL,
			0,
			D3D11_SDK_VERSION,
			&scd,
			pSwapChain.GetAddressOf(),
			pDevice.GetAddressOf(),
			NULL,
			pContext.GetAddressOf()
		);
		gbufA.Create(pDevice.Get(), windowWidth, windowHeight);
		COM_ERROR_IF_FAILED(hr, "Failed to create device and swap chain.");

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));

		COM_ERROR_IF_FAILED(hr, "Failed to get back buffer.");
		D3D11_RENDER_TARGET_VIEW_DESC rvd = {};
		rvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;   // <-- sRGB view
		rvd.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rvd.Texture2D.MipSlice = 0;

		hr = this->pDevice->CreateRenderTargetView(pBackBuffer.Get(), &rvd, pRenderTargetView.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create RTV.");

		CD3D11_TEXTURE2D_DESC depthStencilDesc(DXGI_FORMAT_D24_UNORM_S8_UINT, windowWidth, windowHeight);
		depthStencilDesc.MipLevels = 1;
		depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		hr = this->pDevice->CreateTexture2D(&depthStencilDesc, NULL, this->depthStencilBuffer.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to Depth Stencil.");

		hr = this->pDevice->CreateDepthStencilView(this->depthStencilBuffer.Get(), NULL, this->depthStencilView.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create depth stencil view.");

		this->pContext->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), this->depthStencilView.Get());

		// ----- Depth-stencil state: reversed-Z, writes ON (NO *_EQUAL) -----
		CD3D11_DEPTH_STENCIL_DESC dsRZ(D3D11_DEFAULT);
		dsRZ.DepthEnable = TRUE;
		dsRZ.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsRZ.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; // <? NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&dsRZ, depthStencilState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		CD3D11_DEPTH_STENCIL_DESC dsRZ1(D3D11_DEFAULT);
		dsRZ1.DepthEnable = FALSE;
		dsRZ1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsRZ1.DepthFunc = D3D11_COMPARISON_NEVER; // <? NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&dsRZ1, depthStencilStateLighting.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		D3D11_DEPTH_STENCIL_DESC dd = {};
		dd.DepthEnable = FALSE;
		pDevice->CreateDepthStencilState(&dd, dsDisabled.GetAddressOf());

		// Bind once for the default backbuffer path (your GBuffer has its own dsWrite)
		pContext->OMSetDepthStencilState(depthStencilState.Get(), 0);

		// ----- Rasterizer: back-face culling ON -----
		// Flip FrontCounterClockwise to FALSE if your geometry is clockwise.
		CD3D11_RASTERIZER_DESC rsDesc(D3D11_DEFAULT);
		rsDesc.CullMode = D3D11_CULL_BACK;
		rsDesc.FrontCounterClockwise = TRUE;
		hr = pDevice->CreateRasterizerState(&rsDesc, rasterizerState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer.");

		CD3D11_RASTERIZER_DESC rsDescNoCull = rsDesc;
		rsDescNoCull.CullMode = D3D11_CULL_NONE;
		hr = pDevice->CreateRasterizerState(&rsDescNoCull, rasterizerStateNoCull.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer (nocull).");

		std::filesystem::path font_path = std::filesystem::path(SOLUTION_DIR) / "Data" / "Fonts" / "entropy.spritefont";
		spriteBatch = std::make_unique<DirectX::SpriteBatch>(this->pContext.Get());
		spriteFont = std::make_unique<DirectX::SpriteFont>(
			this->pDevice.Get(),
			font_path.c_str());

		CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

		hr = this->pDevice->CreateSamplerState(&samplerDesc, this->samplerState.GetAddressOf());

		D3D11_BLEND_DESC bd_op = {};
		bd_op.AlphaToCoverageEnable = FALSE;
		bd_op.IndependentBlendEnable = TRUE;

		for (int i = 0; i < 8; ++i)
		{
			D3D11_RENDER_TARGET_BLEND_DESC& rt = bd_op.RenderTarget[i];
			rt.BlendEnable = FALSE;                     // Blend disabled
			rt.SrcBlend = D3D11_BLEND_ONE;
			rt.DestBlend = D3D11_BLEND_ZERO;
			rt.BlendOp = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		hr = pDevice->CreateBlendState(&bd_op, bsOpaque.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create device sampler state.");
		D3D11_BLEND_DESC bd = {};
		bd.AlphaToCoverageEnable = FALSE;
		bd.IndependentBlendEnable = TRUE; // <- as Alkahest does

		for (int i = 0; i < 8; ++i)
		{
			D3D11_RENDER_TARGET_BLEND_DESC rt = {};
			rt.BlendEnable = FALSE; // opaque (no blending)
			rt.SrcBlend = D3D11_BLEND_ONE;
			rt.DestBlend = D3D11_BLEND_ZERO;
			rt.BlendOp = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; // IMPORTANT!
			bd.RenderTarget[i] = rt;
		}
		hr = pDevice->CreateBlendState(&bd, bsGBufferOpaqueIndependent.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create bsGBufferOpaqueIndependent");

		CD3D11_RASTERIZER_DESC rzGBuf(D3D11_DEFAULT);
		rzGBuf.CullMode = D3D11_CULL_BACK;
		rzGBuf.FrontCounterClockwise = TRUE;   // or FALSE if your winding is CW
		rzGBuf.DepthBias = +1;                 // <-- tiny bias in integer units
		rzGBuf.SlopeScaledDepthBias = 0.0f;
		rzGBuf.DepthBiasClamp = 0.0f;
		pDevice->CreateRasterizerState(&rzGBuf, rasterizerStateGBuffer.GetAddressOf());

		D3D11_BLEND_DESC d{}; d.RenderTarget[0].BlendEnable = TRUE;
		d.RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;  d.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		d.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		d.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE; d.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		d.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		d.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		pDevice->CreateBlendState(&d, bsAdditive.GetAddressOf());

		{
			CD3D11_SAMPLER_DESC sd(D3D11_DEFAULT);
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			pDevice->CreateSamplerState(&sd, samplerPointClamp.GetAddressOf());
		}

		// linear-clamp (for color GBuffer)
		{
			CD3D11_SAMPLER_DESC sd(D3D11_DEFAULT);
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			pDevice->CreateSamplerState(&sd, samplerLinearClamp.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sd(D3D11_DEFAULT);
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&sd, this->shading1.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC s(D3D11_DEFAULT);
			s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // trilinear
			s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			s.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&s, shading2.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sl(D3D11_DEFAULT);
			sl.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sl.AddressU = sl.AddressV = sl.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sl.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&sl, lighting1.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC s(D3D11_DEFAULT);
			s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   // trilinear
			s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			s.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&s, lighting2.GetAddressOf());
		}
		
		{
			D3D11_BLEND_DESC bd{};
			bd.RenderTarget[0].BlendEnable = FALSE;
			bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR; // src * dest
			bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			pDevice->CreateBlendState(&bd, bsMultiply.GetAddressOf());
		}

		{
			CD3D11_RASTERIZER_DESC rs(D3D11_DEFAULT);
			rs.CullMode = D3D11_CULL_FRONT;     // draw backfaces of the volume
			rs.FrontCounterClockwise = TRUE;    // keep consistent with your mesh winding
			pDevice->CreateRasterizerState(&rs, rasterizerCullFront.GetAddressOf());
		}

		{
			CD3D11_RASTERIZER_DESC rz(D3D11_DEFAULT);
			rz.FillMode = D3D11_FILL_SOLID;
			rz.CullMode = D3D11_CULL_BACK;
			rz.FrontCounterClockwise = TRUE;      // ? Front CCW
			rz.DepthBias = 5;                     // Depth Bias
			rz.DepthBiasClamp = 1.0e10f;          // Depth Bias Clamp
			rz.SlopeScaledDepthBias = 2.0f;       // Slope-Scaled Bias
			rz.DepthClipEnable = TRUE;            // ? Depth Clip
			rz.ScissorEnable = FALSE;             // ? Scissor
			rz.MultisampleEnable = FALSE;         // ? Multisample
			rz.AntialiasedLineEnable = FALSE;     // ? Line AA

			HRESULT hr = pDevice->CreateRasterizerState(&rz, rasterizerStateBiased.GetAddressOf());
			COM_ERROR_IF_FAILED(hr, "Failed to create biased rasterizer state.");
		}

		{
			D3D11_BLEND_DESC bd{};
			bd.AlphaToCoverageEnable = FALSE;
			bd.IndependentBlendEnable = TRUE;

			// Enable ONE+ONE ADD on both RT0 and RT1
			for (int i = 0; i < 2; ++i) {
				D3D11_RENDER_TARGET_BLEND_DESC rt{};
				rt.BlendEnable = TRUE;
				rt.SrcBlend = D3D11_BLEND_ONE;
				rt.DestBlend = D3D11_BLEND_ONE;
				rt.BlendOp = D3D11_BLEND_OP_ADD;
				rt.SrcBlendAlpha = D3D11_BLEND_ONE;
				rt.DestBlendAlpha = D3D11_BLEND_ONE;
				rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
				rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				bd.RenderTarget[i] = rt;
			}
			// Leave the rest (2..7) disabled/zeroed.
			pDevice->CreateBlendState(&bd, bsAdditive2RT.GetAddressOf());
		}
		D3D11_DEPTH_STENCILOP_DESC dsdesc{};
		dsdesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilFunc = D3D11_COMPARISON_ALWAYS;


		{
			CD3D11_DEPTH_STENCIL_DESC ds(D3D11_DEFAULT);
			ds.DepthEnable = FALSE;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;        // read-only
			ds.DepthFunc = D3D11_COMPARISON_ALWAYS;          // reversed-Z
			ds.StencilReadMask = 0;
			ds.StencilWriteMask = 0;
			ds.BackFace = dsdesc;
			ds.FrontFace = dsdesc;
			pDevice->CreateDepthStencilState(&ds, depthStencilLightVolume.GetAddressOf());
		}

		ComPtr<ID3D11Texture2D> backbuf;
		DXGI_SWAP_CHAIN_DESC scd1 = {}; pSwapChain->GetDesc(&scd1);
		pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuf.GetAddressOf());


		CD3D11_DEPTH_STENCIL_DESC ds_decal(D3D11_DEFAULT);
		ds_decal.DepthEnable = TRUE;
		ds_decal.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		ds_decal.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; // <? NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&ds_decal, depthStencilDecal.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		D3D11_RENDER_TARGET_VIEW_DESC rtvSRGB{};
		rtvSRGB.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvSRGB.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pDevice->CreateRenderTargetView(backbuf.Get(), &rtvSRGB, pRenderTargetView.GetAddressOf());


		D3D11_RENDER_TARGET_VIEW_DESC rtvUNORM{};
		rtvUNORM.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvUNORM.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateRenderTargetView(backbuf.Get(), &rtvUNORM, pRenderTargetViewLinear.GetAddressOf());
	}


	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}

	pool = std::make_unique<ThreadPool>();
	mainQueue = std::make_unique<MainThreadQueue>();
	registry = std::make_unique<RuntimeAssetRegistry>();
	assets = std::make_unique<AssetSystem>(pDevice.Get(), *pool, *mainQueue, registry.get());
	// Create the camera constant buffer

	CreateScopeViewCB12(pDevice.Get());
	CreateCB1(pDevice.Get());
	PublishGlobalChannelsToExterns(externs, channels);
	RenderStates::Create(pDevice.Get(), states);

	return true;
}

bool Graphics::InitializeShaders()
{
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R16G16B16A16_SNORM, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // slot 0, offset 0
		{ "TANGENT",  0, DXGI_FORMAT_R16G16B16A16_SNORM, 0, 8, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // slot 0, offset 8
		{ "TEXCOORD", 0, DXGI_FORMAT_R16G16_SNORM,       1, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }, // slot 1, offset 0
	};

	UINT numElements = ARRAYSIZE(layout);

	if (!this->vertexshader.Initialize(this->pDevice, L"vertexshader.cso", layout, numElements))
		return false;

	if (!this->pixelshader.Initialize(this->pDevice, L"pixelshader.cso"))
		return false;

	Microsoft::WRL::ComPtr<ID3DBlob> vsBytecode;
	HRESULT hr = CompileVSFromMemory(
		pDevice.Get(),
		kVS_SkinnedHack,
		"VSMain",
		"vs_5_0",
		&entity_vs_override,
		vsBytecode.GetAddressOf());
	if (FAILED(hr)) {
		ErrorLogger::Log(hr, "VS compile failed");
		//return false;
	}
	return true;
}

void Graphics::InitializeScopes()
{
	auto& scopes = GlobalData::getScopes();
	for (auto& scope : scopes)
	{
		std::pair<std::string, TigerScope> value;
		TigerScope scope_obj{};
		value.first = scope.first;
		printf("Loading Scope : %08X \n", scope.second.hash);
		auto ScopeTag = bin::parse<SScope>(scope.second.data, scope.second.size, bin::Endian::Little);
		scope_obj.Scope = ScopeTag;
		scope_obj.LoadScopeInfo(pDevice, pContext);
		value.second = scope_obj;
		this->scopes.push_back(value);
	}
}

void Graphics::CreateLightVolumeResources()
{
	// --- Unit cube ([-1,+1]^3), POSITION-only float3
	static const float verts[24][3] = {
	{ 1,1,-1 },
	{ 1,1,1 },
	{ 1,-1,1 },
	{ 1,-1,-1 },
	{ -1,-1,-1 },
	{ -1,-1,1 },
	{ -1,1,1 },
	{ -1,1,-1 },
	{ -1,1,1 },
	{ 1,1,1 },
	{ 1,1,-1 },
	{ -1,1,-1 },
	{ 1,-1,-1 },
	{ 1,-1,1 },
	{ -1,-1,1 },
	{ -1,-1,-1 },
	{ 1,-1,1 },
	{ 1,1,1 },
	{ -1,1,1 },
	{ -1,-1,1 },
	{ -1,-1,-1 },
	{ -1,1,-1 },
	{ 1,1,-1 },
	{ 1,-1,-1 }
	};
	static const uint16_t idx[36] = {
	0, 1, 2, 2, 3, 0, 4, 5, 6, 6, 7, 4,
	8, 9, 10, 10, 11, 8, 12, 13, 14, 14, 15, 12,
	16, 17, 18, 18, 19, 16, 20, 21, 22, 22, 23, 20
	};
	lightCubeIndexCount = (UINT)std::size(idx);

	// VB
	D3D11_BUFFER_DESC vbd{};
	vbd.ByteWidth = UINT(sizeof(verts));
	vbd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	vbd.Usage = D3D11_USAGE_IMMUTABLE;
	D3D11_SUBRESOURCE_DATA vinit{ verts, 0, 0 };
	pDevice->CreateBuffer(&vbd, &vinit, lightCubeVB.GetAddressOf());

	// IB
	D3D11_BUFFER_DESC ibd{};
	ibd.ByteWidth = UINT(sizeof(idx));
	ibd.BindFlags = D3D11_BIND_INDEX_BUFFER;
	ibd.Usage = D3D11_USAGE_IMMUTABLE;
	D3D11_SUBRESOURCE_DATA iinit{ idx, 0, 0 };
	pDevice->CreateBuffer(&ibd, &iinit, lightCubeIB.GetAddressOf());

	// Per-light VS CB (SimpleGeometry.transform)
	D3D11_BUFFER_DESC cbd{};
	cbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	cbd.ByteWidth = sizeof(CBLightSimpleGeometry);
	cbd.Usage = D3D11_USAGE_DYNAMIC;
	cbd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	CBLightSimpleGeometry zero{};
	D3D11_SUBRESOURCE_DATA cinit{ &zero, 0, 0 };
	pDevice->CreateBuffer(&cbd, &cinit, cbLightSimpleGeometry.GetAddressOf());
}

void Graphics::LoadGlobalTextures() {
	auto GlobalTex = GlobalData::getGlobalTextures();
	for (auto tex : GlobalTex) {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textureLoaded;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texSrv;
		auto payload = BuildTexturePayloadFromTag(tex.second);
		D3D11_SHADER_RESOURCE_VIEW_DESC sd;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = 0;
		sd.Texture2D.MipLevels = payload->desc.MipLevels;
		sd.Format = payload->desc.Format;
		HRESULT hr = pDevice->CreateTexture2D(&payload->desc, payload->subresources.data(), &textureLoaded);
		hr = pDevice->CreateShaderResourceView(textureLoaded.Get(), &sd, &texSrv);
		this->global_textures[tex.first] = texSrv;
	}
	HRESULT hr = LoadEmbeddedTextureSRV(pDevice.Get(), 102,/*srgb*/false, this->temp_angle_lookup.GetAddressOf());
	if (FAILED(hr))
		ErrorLogger::Log(hr, "Failed to load baked texture");
}

bool Graphics::InitializeScene()
{
	try {
		camera.SetPosition(3.0f, 0.0f, 0.0f);
		camera.SetProjectionValues(90.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.01f, 10000.0f);
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}
	InitializeInputLayouts();
	Create1x1SRV(0xFFFFFFFF, white1x1SRV);
	Create1x1SRV(0xFF808080, grey1x1SRV);
	InitializeScopes();
	LoadGlobalTextures();
	CreateLightVolumeResources();
	
	loadzone = std::make_unique<LoadZone>(*this);
	loadzone->parentHash = 0x80FC95E9; //duality
	loadzone->ProcessMap();
	this->staticsToDraw = loadzone->statics;
	this->lightsToDraw = loadzone->lights;
	this->entitiesToDraw = loadzone->entities;
	printf("Loading Render Engine\n");
	gTimer.reset();
	return true;
}