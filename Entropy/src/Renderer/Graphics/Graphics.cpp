#include "Graphics.h"
#include <filesystem>
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES



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
	swprintf(label, 128, L"Static geometry %08X", rs.mesh->id);
	//const DirectX::XMMATRIX world = rs.world;
	//printf("Loading Static %08X", rs.mesh->id);
	GpuMarker mark(ctx, label);
	const auto& mesh = *rs.mesh;
	// View constant buffer(s)
	ID3D11Buffer* b1 = g_cb1.Get();            // if you also use cb1 for per-object, leave it bound
	ctx->VSSetConstantBuffers(1, 1, &b1);
	//UploadScopeViewCB12_All(ctx, view, (float)windowWidth, (float)windowHeight);
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

		const auto& tech = part.technique;
		if (!tech) { continue; }
	
		if (!tech->VS.empty()) {

			ID3D11VertexShader* vs = tech->VS[0]->vs.Get();
			ctx->VSSetShader(vs, nullptr, 0);
		}
		if (!tech->PS.empty()) {

			ID3D11PixelShader* ps = tech->PS[0]->ps.Get();
			ctx->PSSetShader(ps, nullptr, 0);
		}


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

		// Make sure b1 is actually bound (once is fine; safe to rebind)
		{ ID3D11Buffer* b = g_cb1.Get(); ctx->VSSetConstantBuffers(1, 1, &b); }

		if (!tech->Textures.empty()) {
			//printf("Mapping Textures");
			const size_t n = std::min(tech->Textures.size(), tech->psTextureSlots.size());
			for (size_t i = 0; i < n; ++i) {
				UINT slot = tech->psTextureSlots[i];
				ID3D11ShaderResourceView* s = tech->Textures[i] ? tech->Textures[i]->Get() : nullptr;
				ctx->PSSetShaderResources(slot, 1, &s);
			}
		}
		if (!tech->Textures3D.empty()) {
			//printf("Mapping 3D Textures");
			const size_t n = std::min(tech->Textures3D.size(), tech->psTextureSlots3D.size());
			for (size_t i = 0; i < n; ++i) {
				UINT slot = tech->psTextureSlots3D[i];
				ID3D11ShaderResourceView* s = tech->Textures3D[i] ? tech->Textures3D[i]->Get() : nullptr;
				ctx->PSSetShaderResources(slot, 1, &s);
			}
		}

		if (!tech->CBuffers.empty()) {
			for (size_t i = 0; i < tech->CBuffers.size(); ++i) {
				ID3D11Buffer* b = tech->CBuffers[i]->buffer.Get();
				ctx->PSSetConstantBuffers(i, 1, &b);
			}
		}

		for (size_t i = 0; i < tech->Samplers.size(); ++i) {
			ID3D11SamplerState* s = tech->Samplers[i]->sampler.Get();
			ctx->PSSetSamplers(i + 1, 1, &s);
		}

		ID3D11ShaderResourceView* s = bg.color.get();
		
		ctx->VSSetShaderResources(0, 1, &s);

		UploadScopeViewCB12_All(ctx, view, float(windowWidth), float(windowHeight));
		float bf[4] = {};
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		// --- Draw
		const UINT instanceCount = (UINT)rs.world.size();
		ctx->DrawIndexedInstanced(part.partInfo.index_count,
			instanceCount,               // InstanceCount
			part.partInfo.index_start,   // StartIndexLocation
			0,                           // BaseVertexLocation
			0);
	}

}

struct CombineCB { float ambient; float pad[3]; };
CombineCB cb = { 0.3f, {0,0,0} }; // or 0.0f if you want NO ambient

void Graphics::CreateWhite1x1SRV()
{
	UINT color = 0xFFFFFFFF; // RGBA8 UNORM white
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	td.SampleDesc = { 1,0 };
	td.Usage = D3D11_USAGE_IMMUTABLE;
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

void Graphics::BlitSRVToBackbuffer(ID3D11ShaderResourceView* srv)
{
	// 1) Pick a built-in blit/copy technique you already have
	std::shared_ptr<EntropyAssets::Technique> tech = nullptr;
	if (auto it = globalTechniques.find("copy_texture_bilinear"); it != globalTechniques.end())
		tech = it->second.get();
	else if (auto it2 = globalTechniques.find("copy_texture"); it2 != globalTechniques.end())
		tech = it2->second.get();
	else if (auto it3 = globalTechniques.find("debug_overlay_blit_texture"); it3 != globalTechniques.end())
		tech = it3->second.get();
	if (!tech || tech->VS.empty() || tech->PS.empty()) {
		printf("Failed to bind vs/ps");
	}
	// 2) Backbuffer + viewport + opaque state
	ID3D11RenderTargetView* bb = pRenderTargetView.Get();
	pContext->OMSetRenderTargets(1, &bb, nullptr);

	D3D11_VIEWPORT vp{}; vp.Width = float(windowWidth); vp.Height = float(windowHeight);
	vp.MinDepth = 0; vp.MaxDepth = 1; pContext->RSSetViewports(1, &vp);

	float bf[4] = {};
	pContext->OMSetDepthStencilState(nullptr, 0);
	pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
	pContext->RSSetState(rasterizerStateNoCull.Get());

	// 3) Bind shaders
	pContext->VSSetShader(tech->VS[0]->vs.Get(), nullptr, 0);
	pContext->PSSetShader(tech->PS[0]->ps.Get(), nullptr, 0);

	// 4) Feed the SRV to the slot this PS expects (usually t0)
	const UINT slot = tech->psTextureSlots.empty() ? 0u : tech->psTextureSlots[0];
	pContext->PSSetShaderResources(slot, 1, &srv);
	ID3D11SamplerState* samp = samplerLinearClamp.Get();
	pContext->PSSetSamplers(0, 1, &samp);

	// 5) Fullscreen strip (no VB/IL; VS must use SV_VertexID)
	pContext->IASetInputLayout(nullptr);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->Draw(4, 0);

	// 6) Unbind SRV
	ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
	pContext->PSSetShaderResources(slot, 1, nullSRV);
}

void Graphics::RunGlobalLightingPass()
{
	wchar_t label[128];
	swprintf(label, 128, L"global_lighting");
	GpuMarker mark(pContext.Get(), label);
	// Bind shaders (or your technique fallback)
	if (auto it = globalTechniques.find("global_lighting"); it != globalTechniques.end()) {
		auto tech = it->second.get();
		pContext->VSSetShader(tech->VS[0]->vs.Get(), nullptr, 0);
		pContext->PSSetShader(tech->PS[0]->ps.Get(), nullptr, 0);
	}

	// cb12 (view) and cb13 (ambient)
	if (ID3D11Buffer* b12 = g_scopeView_b12.Get()) {
		pContext->VSSetConstantBuffers(12, 1, &b12);
		pContext->PSSetConstantBuffers(12, 1, &b12);
	}
	struct { float ambient; float pad[3]; } combine{ 0.05f,{0,0,0} };
	if (!cbGlobalAmbient) {
		D3D11_BUFFER_DESC bd{}; bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(combine); bd.Usage = D3D11_USAGE_DYNAMIC; bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		pDevice->CreateBuffer(&bd, nullptr, cbGlobalAmbient.GetAddressOf());
	}
	D3D11_MAPPED_SUBRESOURCE ms{};
	pContext->Map(cbGlobalAmbient.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &ms);
	memcpy(ms.pData, &combine, sizeof(combine));
	pContext->Unmap(cbGlobalAmbient.Get(), 0);
	ID3D11Buffer* b13 = cbGlobalAmbient.Get();
	pContext->PSSetConstantBuffers(13, 1, &b13);

	// >>> Match Alkahest binding order <<<
	ID3D11ShaderResourceView* srvs[6] = {
	gbufA.rt2.srv.Get(),           // t0  : RT2 (masks)
	gbufA.rt1_read.srv.Get(),      // t1  : RT1_Clone (normal+roughness)
	gbufA.depth.texCopySRV.Get(),  // t2  : depth (R32_FLOAT)
	white1x1SRV.Get(),             // t3  : envDiffuse
	white1x1SRV.Get(),             // t4  : envSpec
	white1x1SRV.Get()              // t5  : BRDF LUT
	};
	pContext->PSSetShaderResources(0, 6, srvs);

	ID3D11SamplerState* sams[2] = { samplerPointClamp.Get(), samplerPointClamp.Get() };
	pContext->PSSetSamplers(0, 2, sams);

	// Fullscreen draw
	pContext->IASetInputLayout(nullptr);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	pContext->GSSetShader(nullptr, nullptr, 0);
	pContext->HSSetShader(nullptr, nullptr, 0);
	pContext->DSSetShader(nullptr, nullptr, 0);
	pContext->Draw(4, 0);

	ID3D11ShaderResourceView* nulls[8] = {};
	pContext->PSSetShaderResources(0, 8, nulls);
}

void Graphics::BeginLightingPass()
{
	// Unbind any SRVs that might alias the light RTs
	ID3D11ShaderResourceView* nulls[16] = {};
	pContext->PSSetShaderResources(0, 16, nulls);

	ID3D11RenderTargetView* mrt[3] = {
		gbufA.light_diffuse.rtv.Get(),
		gbufA.light_specular.rtv.Get(),
		gbufA.light_ibl_specular.rtv.Get()
	};
	pContext->OMSetRenderTargets(3, mrt, nullptr);

	D3D11_VIEWPORT vp{}; vp.Width = float(gbufA.w); vp.Height = float(gbufA.h);
	vp.MinDepth = 0; vp.MaxDepth = 1; pContext->RSSetViewports(1, &vp);

	pContext->OMSetDepthStencilState(nullptr, 0);

	float bf[4] = {};
	pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xffffffff);  // <- force opaque
	pContext->RSSetState(rasterizerStateNoCull.Get());

	const float eps[4] = { 0.001f,0.001f,0.001f,0.0f }; // diffuse seed (or use 0 if you prefer)
	const float zero[4] = { 0,0,0,0 };
	pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), eps);
	pContext->ClearRenderTargetView(gbufA.light_specular.rtv.Get(), zero);
	if (gbufA.light_ibl_specular.rtv)
		pContext->ClearRenderTargetView(gbufA.light_ibl_specular.rtv.Get(), zero);
}
void Graphics::EndLightingPass()
{
	ID3D11RenderTargetView* nullRT[3] = { nullptr, nullptr, nullptr };
	pContext->OMSetRenderTargets(3, nullRT, nullptr);
}

void Graphics::RenderFrame()
{
	mainQueue->Drain();

	// View/Proj
	View viewState{};
	XMStoreFloat4x4(&viewState.world_to_camera, camera.GetViewMatrix());
	XMStoreFloat4x4(&viewState.camera_to_projective, camera.GetProjectionMatrix());
	viewState.derive_matrices_vs({ {float(windowWidth), float(windowHeight)} });
	UploadScopeViewCB12_All(pContext.Get(), viewState, float(windowWidth), float(windowHeight));

	gbufA.BindGBufferForWriting(pContext.Get());
	pContext->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0);
	pContext->RSSetState(rasterizerState.Get());
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pipelineStage = PipelineStage::GBuffer;
	for (auto& rs : staticsToDraw) DrawStaticMesh(rs, viewState);

	// Finalize GBuffer for reading
	gbufA.UnbindMRTs(pContext.Get());
	pContext->CopyResource(gbufA.rt1_read.tex.Get(), gbufA.rt1.tex.Get());     // normals copy
	pContext->CopyResource(gbufA.depth.texCopy.Get(), gbufA.depth.tex.Get());  // depth copy

	// 2) Global lighting only
	BeginLightingPass();
	const float red[4] = { 1,0,0,0 };
	pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), red);
	RunGlobalLightingPass();
	EndLightingPass();

	// after EndLightingPass()
	pContext->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), nullptr);
	const float black[4] = { 0,0,0,1 };
	pContext->ClearRenderTargetView(pRenderTargetView.Get(), black);

	// draw the light buffer to the screen
	BlitSRVToBackbuffer(gbufA.light_diffuse.srv.Get());

	// Unbind SRVs so these textures can become RTVs next frame again
	ID3D11ShaderResourceView* nulls[16] = {};
	pContext->PSSetShaderResources(0, 16, nulls);
	// ---------- 3) UI ----------
	if (!fpsTimer.isrunning) { fpsTimer.Start(); }
	static int fpsCounter = 0;
	static std::string fpsString = "FPS: 0";
	fpsCounter++;
	if (fpsTimer.GetMilisecondsElapsed() > 1000) {
		fpsString = "FPS: " + std::to_string(fpsCounter);
		fpsCounter = 0;
		fpsTimer.Restart();
	}

	auto CameraPos = camera.GetPositionFloat3();
	std::string CameraPrint = std::format("X: {:.2f}  Y: {:.2f}  Z: {:.2f}", CameraPos.x, CameraPos.y, CameraPos.z);
	auto CameraRot = camera.GetRotationFloat3();
	std::string CameraPrintRot = std::format("Pitch: {:.2f}  Roll: {:.2f}  Yaw: {:.2f}", CameraRot.x, CameraRot.y, CameraRot.z);
	static bool drawrt1 = false;
	static bool drawrt0 = false;
	static bool drawrt2 = false;
	static bool drawLight_diffuse = false;
	static bool drawLight_specular = false;
	spriteBatch->Begin();
	if (drawrt1) {
		spriteBatch->Draw(gbufA.rt1_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	}
	if (drawrt0) {
		spriteBatch->Draw(gbufA.rt0.srv.Get(), DirectX::XMFLOAT2(-1, -1));
	}
	if (drawrt2) {
		spriteBatch->Draw(gbufA.rt2.srv.Get(), DirectX::XMFLOAT2(0, 0));
	}
	if (drawLight_diffuse) {
		spriteBatch->Draw(gbufA.light_diffuse.srv.Get(), DirectX::XMFLOAT2(0, 0));
	}
	if (drawLight_specular) {
		spriteBatch->Draw(gbufA.light_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	}
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(),
		DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(),
		DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrintRot).c_str(),
		DirectX::XMFLOAT2(0, 100), DirectX::Colors::Wheat);
	spriteBatch->End();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test");
	static float value = 50.0f;
	if (ImGui::DragFloat("Speed X:", &value, 1, 0.0f, 100.0f, "%.0f%%")) {
		camera.SetSpeed(value / 10.0f);
	}
	
	ImGui::Checkbox("Draw gbuffer", &drawrt0);
	ImGui::Checkbox("Draw Rt1", &drawrt1);
	ImGui::Checkbox("Draw Rt2", &drawrt2);
	ImGui::Checkbox("Draw Light_diffuse", &drawLight_diffuse);
	ImGui::Checkbox("Draw Light_specular", &drawLight_specular);
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

		CD3D11_DEPTH_STENCIL_DESC depthstencildesc(D3D11_DEFAULT);
		depthstencildesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;

		hr = this->pDevice->CreateDepthStencilState(&depthstencildesc, this->depthStencilState.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create depth stencil state.");


		CD3D11_VIEWPORT viewport(0.0f, 0.0f, float(this->windowWidth), float(this->windowHeight));
		pContext->RSSetViewports(1, &viewport);

		CD3D11_RASTERIZER_DESC rasterizerDesc(D3D11_DEFAULT);
		rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
		rasterizerDesc.FrontCounterClockwise = FALSE;

		hr = this->pDevice->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());

		CD3D11_RASTERIZER_DESC rasterizerDesc2(D3D11_DEFAULT);
		rasterizerDesc2.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
		rasterizerDesc2.FrontCounterClockwise = FALSE;

		hr = this->pDevice->CreateRasterizerState(&rasterizerDesc2, this->rasterizerStateNoCull.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer.");

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

		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;

		auto& rt = desc.RenderTarget[0];
		rt.BlendEnable = FALSE;                          // <-- this line
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		pDevice->CreateBlendState(&desc, &bsOpaque);

		COM_ERROR_IF_FAILED(hr, "Failed to create device sampler state.");

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
	CreateWhite1x1SRV();
	
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
	staticMap = std::make_unique<StaticMap>(*this);
	staticMap->Initialize(0x8102A565);  // root map hash (or whatever yours is)
	staticsToDraw = staticMap->GetRenderList();
	return true;
}
