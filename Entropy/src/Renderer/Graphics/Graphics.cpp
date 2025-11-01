#include "Graphics.h"
#include <filesystem>
#include <d3dcompiler.h>
#include "Scope/AtmosphereExternUI.h"
#include "Scope/FrameGlobalLightingExternUI.h"
#include "Scope/global_channel_ui.h"
#pragma comment(lib, "d3dcompiler.lib")
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

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



void Graphics::CreateWhite1x1SRV()
{
	UINT color = 0xFFFFFFFF; // RGBA8 UNORM white
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

	pDevice->CreateShaderResourceView(tex.Get(), &svd, white1x1SRV.GetAddressOf());
}

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
		ID3D11RenderTargetView* rts[3] = {
	gbufA.rt0.rtv.Get(),
	gbufA.rt1.rtv.Get(),
	gbufA.rt2.rtv.Get()
		};
		ctx->OMSetRenderTargets(3, rts, gbufA.depth.dsv.Get());

		float bf[4] = { 1.0f,1.0f, 1.0f, 1.0f };
		ctx->OMSetBlendState(bsGBufferOpaqueIndependent.Get(), bf, 0xFFFFFFFF);
		ctx->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0); // GREATER, writes ON
			// Bind exactly the MRTs you have:
		ctx->VSSetConstantBuffers(1, 1, &b1);
		const UINT instanceCount = (UINT)rs.world.size();
		ctx->DrawIndexedInstanced(part.partInfo.index_count,
			instanceCount,               // InstanceCount
			part.partInfo.index_start,   // StartIndexLocation
			0,                           // BaseVertexLocation
			0);
	}

}
struct CB13Data { DirectX::XMFLOAT4 v0, v1, v2, v3, v4; };

void Graphics::CreateCB13()
{
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(CB13Data);
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	// Initial values from your screenshot
	CB13Data init{
		{ 69.9002f, 69.9002f, 0.04437f,   1.0f },       // cb13_v0
		{  1.0f,    16.0f,    1.0f,       1.0f },       // cb13_v1
		{130.46442f,124.48499f,1141.70886f,239.59105f },// cb13_v2
		{  0.50f,    0.50f,    0.50f,     0.0f },       // cb13_v3  (last component unknown -> 0)
		{  1.0f,     1.0f,     0.0f,      1.0f }        // cb13_v4
	};

	D3D11_SUBRESOURCE_DATA srd{ &init, 0, 0 };
	COM_ERROR_IF_FAILED(pDevice->CreateBuffer(&bd, &srd, cb13_.GetAddressOf()),
		"Create CB13 failed");
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
	const float fovY = DirectX::XMConvertToRadians(90.0f);
	const float aspect = float(windowWidth) / float(windowHeight);
	DirectX::XMMATRIX projRZ = MakeReversedZProjLH(fovY, aspect, camera.GetNearZ());
	XMStoreFloat4x4(&viewState.camera_to_projective, projRZ);

	viewState.derive_matrices_vs({ {float(windowWidth), float(windowHeight)} });


	{
		// View (needs to exist before you draw statics into the GBuffer)
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;

		externs.SetViewProjectiveToCamera(viewState, vp);

		// Optional, but nice defaults that some shaders might read
		{
			// ShadowMask: (W,H, 1/W,1/H)
			const Vec4 smParams(
				float(windowWidth), float(windowHeight),
				windowWidth ? 1.0f / float(windowWidth) : 0.0f,
				windowHeight ? 1.0f / float(windowHeight) : 0.0f
			);
			externs.SetShadowMaskParams(&smParams, nullptr);
		}
		

		// Make sure CBs exist and push current bytes so the GBuffer pass can use them
		externs.EnsureAll(pDevice.Get());
		externs.UploadAll(pContext.Get());
	}
	for (auto& scope : scopes)
	{
		scope.second.UpdateScopeBuffers(pContext, externs);
	}
	// =========================
	// generate_gbuffer
	// =========================
	{
		ScopedGpuEvent e(anno_.Get(), L"generate_gbuffer");

		// Make sure nothing aliases MRTs
		ID3D11ShaderResourceView* nullSRV[16] = {};
		pContext->PSSetShaderResources(0, 16, nullSRV);

		// Bind GBuffer (RT0/RT1/RT2 + DSV) and clear them (also sets viewport)
		gbufA.BindGBufferForWriting(pContext.Get());

		float bf[4] = { 1.0f,1.0f,1.0f,1.0f };;
		pContext->OMSetBlendState(bsGBufferOpaqueIndependent.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0);

		pContext->RSSetState(rasterizerState.Get());
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		pipelineStage = PipelineStage::GBuffer;

		for (auto& rs : staticsToDraw) {
			DrawStaticMesh(rs, viewState);
		}
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (RT1->RT1_Clone)");
		pContext->CopyResource(gbufA.rt1_read.tex.Get(), gbufA.rt1.tex.Get());
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (Depth->DepthCopySRV)");
		// Copy to the GPU-readable copy so we can sample depth in lighting
		if (gbufA.depth.texCopy && gbufA.depth.tex)
			pContext->CopyResource(gbufA.depth.texCopy.Get(), gbufA.depth.tex.Get());
	}
	{ //decals
		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
		for (auto& rs : staticsToDraw) {
			if (rs.mesh->id != 0x80AC5E06) {
				//continue;
			}
				
			DrawStaticSpecial(rs, viewState);
		}

	}
	{
		ScopedGpuEvent e(anno_.Get(), L"lighting_pass");

		ID3D11RenderTargetView* rts[] = {
			gbufA.light_diffuse.rtv.Get(),
			gbufA.light_specular.rtv.Get(),
		};
		pContext->OMSetRenderTargets(2, rts, nullptr);

		// 1) Bind technique first (shaders + any internal state it sets)
		if (auto it = globalTechniques.find("global_lighting"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		// 2) Now override with the states we want for this pass (like Alkahest)
		// depth disabled (no DSV), rasterizer set, viewport set
		pContext->OMSetDepthStencilState(depthStencilStateLighting.Get(), 0); // DepthEnable=FALSE
		pContext->RSSetState(rasterizerStateNoCull.Get());

		D3D11_VIEWPORT vp{ 0, 0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);

		//// Clear RTVs (Alkahest clears diffuse to a tiny value; use a bright debug first)
		//const float dbg[4] = { 0.001,0.001,0.001,0 };       // TEMP: visible red to verify in RD immediately
		//const float black[4] = { 0,0,0,0 };
		//pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), dbg);
		//pContext->ClearRenderTargetView(gbufA.light_specular.rtv.Get(), black);

		// PS resources (t0..t5) like Alkahest
		ID3D11ShaderResourceView* srvs[] = {
			gbufA.rt2.srv.Get(),           // t0 material/params
			gbufA.rt1_read.srv.Get(),      // t1 normal+roughness
			gbufA.depth.texCopySRV.Get(),  // t2 depth (R32_FLOAT)
			white1x1SRV.Get(),             // t3
			white1x1SRV.Get(),             // t4
			white1x1SRV.Get(),             // t5
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs), srvs);

		// Opaque blend, like RD’s “Blend State 46”
		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);

		// (Alkahest sets DS & RS again; harmless but we keep the order tight)
		pContext->OMSetDepthStencilState(depthStencilStateLighting.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		ID3D11SamplerState* sams2[] = {
			lighting1.Get(),  // s0 : point-clamp (no filtering across GBuffer texels)
			lighting2.Get()  // s1 : linear for BRDF LUT
		};
		pContext->PSSetSamplers(0, (UINT)std::size(sams2), sams2);

		// Fullscreen triangle strip (VS must generate vertices; your technique should do this)
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
	}

	{
		ScopedGpuEvent e(anno_.Get(), L"deferred_shading");

		ID3D11RenderTargetView* rt = gbufA.shading_result.rtv.Get();
		pContext->OMSetRenderTargets(1, &rt, nullptr);

		// 1) Bind technique FIRST (it sets VS/PS and may set states)
		if (auto it = globalTechniques.find("deferred_shading_no_atm"); it != globalTechniques.end())  //80C0D03B
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		// 2) Force pass states (fullscreen, no depth, opaque)
		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		D3D11_VIEWPORT vp{ 0,0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);

		// >>> Correct SRV binding order for THIS PS <<<
		ID3D11ShaderResourceView* srvs2[] = {
			gbufA.rt1_read.srv.Get(),        // t0 : NORMALS  (RT1)  <-- was wrong before
			gbufA.rt0.srv.Get(),             // t1 : ALBEDO   (RT0)  <-- swap with t0
			gbufA.rt2.srv.Get(),             // t2 : MATERIAL (RT2)
			gbufA.light_diffuse.srv.Get(),   // t3 : light diffuse
			gbufA.light_specular.srv.Get(),  // t4 : light specular
			gbufA.light_ibl_specular.srv.Get(), // t5 : IBL specular
			this->global_textures.find("iridescence_lookup_texture")->second.Get(), // t6 : BRDF LUT (fallback to white if needed)
			white1x1SRV.Get(),  // t7 : AO/shadow mask (fallback)
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs2), srvs2);

		// Samplers: the PS uses s0 for most textures, s1 for the BRDF LUT (sample_l)
		ID3D11SamplerState* sams2[] = {
			shading1.Get(),  // s0 : point-clamp (no filtering across GBuffer texels)
			shading2.Get()  // s1 : linear for BRDF LUT
		};
		pContext->PSSetSamplers(0, (UINT)std::size(sams2), sams2);

		// Fullscreen triangle
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
	}

	{
		ScopedGpuEvent e(anno_.Get(), L"present_to_backbuffer");

		// bind backbuffer RTV
		ID3D11RenderTargetView* bb = pRenderTargetView.Get();
		pContext->OMSetRenderTargets(1, &bb, nullptr);

		// viewport
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &vp);

		// clear (okay to clear once, BEFORE the blit)
		const float clear[4] = { 0,0,0,1 };
		pContext->ClearRenderTargetView(bb, clear);

		// FIX #1: bind the technique FIRST (it may set DS/RS/blend internally)
		if (auto it = globalTechniques.find("final_combine"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		// FIX #2: now FORCE the states we require for this pass
		// (depth OFF since no DSV, opaque writes, no cull)
		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		// show Light_Diffuse for now (swap to shading_result later)
		ID3D11ShaderResourceView* src = gbufA.shading_result.srv.Get();
		pContext->PSSetShaderResources(0, 1, &src);
		ID3D11SamplerState* samp = samplerLinearClamp.Get();
		pContext->PSSetSamplers(0, 1, &samp);

		// fullscreen draw (triangle strip 4 verts; VS must generate positions)
		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		// unbind to avoid hazards
		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		pContext->PSSetShaderResources(0, 1, nullSRV);
	}


	if (!fpsTimer.isrunning) { fpsTimer.Start(); }
	static int fpsCounter = 0; static std::string fpsString = "FPS: 0";
	if (++fpsCounter, fpsTimer.GetMilisecondsElapsed() > 1000) {
		fpsString = "FPS: " + std::to_string(fpsCounter);
		fpsCounter = 0; fpsTimer.Restart();
	}

	auto CameraPos = camera.GetPositionFloat3();
	std::string CameraPrint = std::format("X: {:.2f}  Y: {:.2f}  Z: {:.2f}", CameraPos.x, CameraPos.y, CameraPos.z);
	auto CameraRot = camera.GetRotationFloat3();
	std::string CameraPrintRot = std::format("Pitch: {:.2f}  Roll: {:.2f}  Yaw: {:.2f}", CameraRot.x, CameraRot.y, CameraRot.z);

	static bool drawrt1 = false, drawrt0 = false, drawrt2 = false, drawLight_diffuse = false, drawLight_specular = false, drawDepth = false, drawShading= false, drawLight_ibl =false;
	//pContext->OMSetBlendState(bsOpaque.Get(),black
	spriteBatch->Begin();
	if (drawrt1)         spriteBatch->Draw(gbufA.rt1_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt0)         spriteBatch->Draw(gbufA.rt0.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt2)         spriteBatch->Draw(gbufA.rt2.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_diffuse)   spriteBatch->Draw(gbufA.light_diffuse.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_specular)  spriteBatch->Draw(gbufA.light_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_ibl)  spriteBatch->Draw(gbufA.light_ibl_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawShading)  spriteBatch->Draw(gbufA.shading_result.srv.Get(), DirectX::XMFLOAT2(0, 0));
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(), DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(), DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrintRot).c_str(), DirectX::XMFLOAT2(0, 100), DirectX::Colors::Wheat);
	spriteBatch->End();
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Debug Menu");
	static float value = 50.0f;
	if (ImGui::DragFloat("Speed X:", &value, 1, 0.0f, 100.0f, "%.0f%%")) {
		camera.SetSpeed(value / 10.0f);
	}
	ImGui::Checkbox("Draw gbuffer", &drawrt0);
	ImGui::Checkbox("Draw Rt1", &drawrt1);
	ImGui::Checkbox("Draw Rt2", &drawrt2);
	ImGui::Checkbox("Draw Light_diffuse", &drawLight_diffuse);
	ImGui::Checkbox("Draw Light_specular", &drawLight_specular);
	ImGui::Checkbox("Draw Depth", &drawDepth);
	ImGui::Checkbox("Draw Shading", &drawShading);
	ImGui::Checkbox("Draw drawLight_ibl", &drawLight_ibl);
	if (ImGui::CollapsingHeader("ViewExtern"))
	{
		ViewExtern v{};
		if (TryGetViewExtern(externs, v))
		{
			ImGui::Text("Res: %.0f x %.0f", v.resolution_width, v.resolution_height);
			ImGui::Text("Pos: (%.3f, %.3f, %.3f, %.3f)", v.position.x, v.position.y, v.position.z, v.position.w);
			ImGui::Text("unk30: (%.3f, %.3f, %.3f, %.3f)", v.unk30.x, v.unk30.y, v.unk30.z, v.unk30.w);

			ShowMat("world_to_camera", v.world_to_camera);
			ShowMat("camera_to_projective", v.camera_to_projective);
			ShowMat("camera_to_world", v.camera_to_world);
			ShowMat("world_to_projective", v.world_to_projective);
			ShowMat("projective_to_world", v.projective_to_world);
			ShowMat("projective_to_camera", v.projective_to_camera);
			ShowMat("target_pixel_to_camera", v.target_pixel_to_camera);
			ShowMat("target_pixel_to_world", v.target_pixel_to_world);
			ShowMat("tptow_no_proj_w", v.tptow_no_proj_w);
		}
		else {
			ImGui::TextDisabled("ViewExtern not set.");
		}
	}
	// In your ImGui debug window:
	if (ImGui::CollapsingHeader("Atmosphere"))
	{
		if (ImGui::Button("Ensure + Defaults (once)")) {
			EnsureAtmosphereCapacity(externs);
			AtmosphereSetDefaults(externs);
		}
		bool changed = ShowAtmosphereExternEditor(externs);

		// If you want changes to take effect immediately this frame:
		if (changed) {
			// If you already call externs.UploadAll() before the passes next frame,
			// you can skip this; otherwise do it now with the current context.
			externs.Upload(/*ID3D11DeviceContext* */ pContext.Get(), TfxExtern::Atmosphere);
		}
	}
	if (ImGui::CollapsingHeader("Frame")) {
		if (ImGui::Button("Ensure + Defaults (once)")) { EnsureFrameCapacity(externs); FrameSetDefaults(externs); }
		bool changed = ShowFrameExternEditor(externs);
		if (changed) externs.Upload(pContext.Get(), TfxExtern::Frame);
	}

	if (ImGui::CollapsingHeader("GlobalLighting")) {
		if (ImGui::Button("Ensure + Defaults (once)")) { EnsureGlobalLightingCapacity(externs); GlobalLightingSetDefaults(externs); }
		bool changed = ShowGlobalLightingExternEditor(externs);
		if (changed) externs.Upload(pContext.Get(), TfxExtern::GlobalLighting);
	}
	if (ImGui::CollapsingHeader("Global Channels"))
	{
		bool changed = ShowGlobalChannelsEditor(channels, externs, /*autoPublish*/true);
		if (changed) {
			// If you don’t already call UploadAll later, do a targeted upload here:
			externs.Upload(pContext.Get(), TfxExtern::Generic);
		}
	}
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
			CD3D11_SAMPLER_DESC s2(D3D11_DEFAULT);
			s2.Filter = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
			s2.AddressU = s2.AddressV = s2.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			s2.MipLODBias = -0.5f;
			pDevice->CreateSamplerState(&s2, shading2.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sl(D3D11_DEFAULT);
			sl.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sl.AddressU = sl.AddressV = sl.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sl.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&sl, lighting1.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sl2(D3D11_DEFAULT);
			sl2.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sl2.AddressU = sl2.AddressV = sl2.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sl2.MipLODBias = -0.5f;
			pDevice->CreateSamplerState(&sl2, lighting2.GetAddressOf());
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

		CD3D11_DEPTH_STENCIL_DESC ds_decal(D3D11_DEFAULT);
		ds_decal.DepthEnable = TRUE;
		ds_decal.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		ds_decal.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; // <? NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&ds_decal, depthStencilDecal.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");
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
	CreateCB13();
	CreateWhite1x1SRV();
	InitializeScopes();
	LoadGlobalTextures();
	staticMap = std::make_unique<StaticMap>(*this);
	staticMap->Initialize(0x80FBAE28);  // root map hash (or whatever yours is)
	staticsToDraw = staticMap->GetRenderList();

	gTimer.reset();
	return true;
}