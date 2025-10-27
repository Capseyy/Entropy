#include "Graphics.h"
#include <filesystem>
#include <d3dcompiler.h>
#pragma comment(lib, "d3dcompiler.lib")
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES


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

		// Make sure b1 is actually bound (once is fine; safe to rebind)
		{ ID3D11Buffer* b = g_cb1.Get(); ctx->VSSetConstantBuffers(1, 1, &b); }
		ID3D11ShaderResourceView* s = bg.color.get();
		//ctx->VSSetShaderResources(1, 1, &s);
		ctx->VSSetShaderResources(0, 1, &s);
		pContext->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0);

		tech->Bind(pDevice, pContext, externs, states);
		
		// --- Re-assert OM targets and states for the GBuffer pass ---
		if (pipelineStage == PipelineStage::GBuffer) {
			// Bind exactly the MRTs you have:
			ID3D11RenderTargetView* rts[3] = {
	gbufA.rt0.rtv.Get(),
	gbufA.rt1.rtv.Get(),
	gbufA.rt2.rtv.Get()
			};
			ctx->OMSetRenderTargets(3, rts, gbufA.depth.dsv.Get());

			float bf[4] = {1.0f,1.0f, 1.0f, 1.0f};
			ctx->OMSetBlendState(bsGBufferOpaqueIndependent.Get(), bf, 0xFFFFFFFF);
			ctx->OMSetDepthStencilState(gbufA.depth.dsWrite.Get(), 0); // GREATER, writes ON
		}
		const UINT instanceCount = (UINT)rs.world.size();
		ctx->DrawIndexedInstanced(part.partInfo.index_count,
			instanceCount,               // InstanceCount
			part.partInfo.index_start,   // StartIndexLocation
			0,                           // BaseVertexLocation
			0);
	}
	//swprintf(label, 128, L"Static geometry decal %08X", rs.mesh->id);
	//mark.a(label);
	for (const auto& special : rs.specials) {                   
		continue;
		// If technique is shared_ptr:
		auto tech = special->technique;               // copy is fine; bumps refcount
		//if (!tech) continue;

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

		ID3D11Buffer* vbs[3];
		UINT          strides[3];
		UINT          offsets[3] = { 0,0,0 };
		UINT          vbCount = 0;
		auto& bg = special->group;

		if (bg->vertex) { vbs[vbCount] = bg->vertex.get(); strides[vbCount] = bg->vertexStride; ++vbCount; }
		if (bg->uv) { vbs[vbCount] = bg->uv.get();     strides[vbCount] = bg->uvStride;     ++vbCount; }
		//if (bg.color) { vbs[vbCount] = bg.color.get();  strides[vbCount] = bg.colorStride;  ++vbCount; }

		ctx->IASetVertexBuffers(0, vbCount, vbs, strides, offsets);
		ctx->IASetIndexBuffer(bg->index.get(), bg->indexFormat, 0);

		// Make sure b1 is actually bound (once is fine; safe to rebind)
		{ ID3D11Buffer* b = g_cb1.Get(); ctx->VSSetConstantBuffers(1, 1, &b); }
		ID3D11ShaderResourceView* s = bg->color.get();
		//ctx->VSSetShaderResources(1, 1, &s);
		ctx->VSSetShaderResources(0, 1, &s);
		tech->Bind(pDevice, pContext, externs, states);
		UploadScopeViewCB12_All(ctx, view, float(windowWidth), float(windowHeight));
		float bf[4] = {1.0f,1.0f,1.0f,1.0f};
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		// --- Draw
		const UINT instanceCount = (UINT)rs.world.size();
		ctx->DrawIndexedInstanced(special->part.index_count,
			instanceCount,               // InstanceCount
			special->part.index_start,   // StartIndexLocation
			0,                           // BaseVertexLocation
			0);
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
	UploadScopeViewCB12_All(pContext.Get(), viewState, float(windowWidth), float(windowHeight));

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

	// =========================
	// copy (RT1 -> RT1_Clone) and depth -> depthCopy
	// =========================
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

	// Unbind MRTs to avoid hazards before lighting
	gbufA.UnbindMRTs(pContext.Get());

	// =========================
	// decals (optional)
	// =========================
	// ...bind rt0/rt1/rt2 again and render decals if you have them...

	// =========================
	// lighting_pass
	// =========================
	{
		ScopedGpuEvent e(anno_.Get(), L"lighting_pass");

		// Bind lighting outputs (3 RTs like Alkahest)
		ID3D11RenderTargetView* lightRTs[3] = {
			gbufA.light_diffuse.rtv.Get(),
			gbufA.light_specular.rtv.Get(),
			gbufA.shading_result.rtv.Get()
		};
		pContext->OMSetRenderTargets(3, lightRTs, nullptr);

		// Clear as in the capture: diffuse ~ 0.001, others 0
		const float cDiffuse[4] = { 0.001f, 0.001f, 0.001f, 0.0f };
		const float cBlack[4] = { 0.0f,   0.0f,   0.0f,   0.0f };
		pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), cDiffuse);
		pContext->ClearRenderTargetView(gbufA.light_specular.rtv.Get(), cDiffuse);
		pContext->ClearRenderTargetView(gbufA.shading_result.rtv.Get(), cBlack);
		pContext->RSSetState(rasterizerStateNoCull.Get());
		float bf[4] = { 1.0f,1.0f, 1.0f, 1.0f};
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);

		// Depth testing is not needed for a full-screen pass; we sample depth via SRV.
		pContext->OMSetDepthStencilState(nullptr, 0);

		// Bind G-buffer SRVs for the pixel shader:
		ID3D11ShaderResourceView* gbufSRVs[3] = {
			gbufA.rt2.srv.Get(),             // Albedo (sRGB->linear in shader)
			gbufA.rt1.srv.Get(),             // Normal/Roughness            // Material / params
			gbufA.depth.texCopySRV.Get()     // Linear depth SRV (reversed-Z)
		};
		pContext->PSSetShaderResources(0, 3, gbufSRVs);

		// Bind samplers you need (point/linear clamp)
		ID3D11SamplerState* sams[2] = { samplerPointClamp.Get(), samplerLinearClamp.Get() };
		pContext->PSSetSamplers(0, 2, sams);

		// Bind lighting technique/shaders (replace with yours)
		if (auto it = globalTechniques.find("global_lighting"); it != globalTechniques.end()) {
			it->second.get()->Bind(pDevice, pContext, externs, states);
			printf("written global lighting");
		}
		
		pContext->OMSetDepthStencilState(depthStencilStateLighting.Get(), 0);
		// Draw full-screen to accumulate lighting
		DrawFullscreenTriangle(pContext.Get());

		// Unbind SRVs to avoid “resource is still bound” hazards later
		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);
	}

	// =========================
	// Present / debug overlay
	// =========================
	pContext->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), nullptr);
	D3D11_VIEWPORT vp{};
	vp.Width = float(windowWidth); vp.Height = float(windowHeight);
	vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
	pContext->RSSetViewports(1, &vp);

	const float black[4] = { 0,0,0,1 };
	pContext->ClearRenderTargetView(pRenderTargetView.Get(), black);

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

	static bool drawrt1 = false, drawrt0 = false, drawrt2 = false, drawLight_diffuse = false, drawLight_specular = false, drawDepth = false;

	spriteBatch->Begin();
	if (drawrt1)         spriteBatch->Draw(gbufA.rt1_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt0)         spriteBatch->Draw(gbufA.rt0.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt2)         spriteBatch->Draw(gbufA.rt2.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_diffuse)   spriteBatch->Draw(gbufA.light_diffuse.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_specular)  spriteBatch->Draw(gbufA.light_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	//if (drawDepth)           spriteBatch->Draw(gbufA.depth.srv.Get(), DirectX::XMFLOAT2(0, 0));
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
		dsRZ.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; // <— NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&dsRZ, depthStencilState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		CD3D11_DEPTH_STENCIL_DESC dsRZ1(D3D11_DEFAULT);
		dsRZ1.DepthEnable = FALSE;
		dsRZ1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsRZ1.DepthFunc = D3D11_COMPARISON_NEVER; // <— NOT GREATER_EQUAL
		hr = pDevice->CreateDepthStencilState(&dsRZ1, depthStencilStateLighting.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");


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

		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;

		// Easiest: make the same blend for all MRTs
		desc.IndependentBlendEnable = TRUE;

		D3D11_RENDER_TARGET_BLEND_DESC rt = {};
		rt.BlendEnable = FALSE; // opaque
		rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

		// When IndependentBlendEnable == FALSE, only [0] is used,
		// but it’s harmless (and future-proof) to fill them all:
		for (int i = 0; i < 8; ++i) desc.RenderTarget[i] = rt;

		pDevice->CreateBlendState(&desc, &bsOpaque);

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
	staticMap->Initialize(0x80AD0BBD);  // root map hash (or whatever yours is)
	staticsToDraw = staticMap->GetRenderList();
	gTimer.reset();
	return true;
}
