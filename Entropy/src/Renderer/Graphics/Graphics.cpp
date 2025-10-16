#include "Graphics.h"
#include <filesystem>


static inline float asfloat_u32(std::uint32_t u) {
	return std::bit_cast<float>(u);
}


// Max instances you plan to send per draw (<= 63 to stay within 4KB)
constexpr uint32_t MAX_INST = 1;

// Raw cb1 payload = 32 bytes header + 64 bytes per instance * MAX_INST.
// 32 + 64*63 = 4064 bytes (fits <= 4096).
struct CB1Payload {
	// cb1[0]
	DirectX::XMFLOAT4 meshOffset_meshScale;   // xyz, w
	// cb1[1]
	DirectX::XMFLOAT4 uvScale_uvOffset;       // x, y, z, w(as float)
	// cb1[2..] rows for instances
	DirectX::XMFLOAT4 rows[4 * MAX_INST];     // 4 float4 per instance
};

Microsoft::WRL::ComPtr<ID3D11Buffer> g_cb1;

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

void UpdateCB1(
	ID3D11DeviceContext* ctx,
	const DirectX::XMFLOAT3& meshOffset, float meshScale,
	float uvScaleX, float uvOffX, float uvOffY, std::uint32_t maxColorOrClampBits,
	const std::vector<DirectX::XMMATRIX>& worlds // size <= MAX_INST
) {
	using namespace DirectX;

	D3D11_MAPPED_SUBRESOURCE m{};
	ctx->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
	auto* cb = reinterpret_cast<CB1Payload*>(m.pData);

	// Header
	cb->meshOffset_meshScale = XMFLOAT4(meshOffset.x, meshOffset.y, meshOffset.z, meshScale);
	cb->uvScale_uvOffset = XMFLOAT4(uvScaleX, uvOffX, uvOffY, asfloat_u32(maxColorOrClampBits));

	// Per-instance rows
	const XMFLOAT4 flagRow(1.f, 1.f, 1.f, asfloat_u32(0x02000000u));
	const uint32_t count = (uint32_t)std::min<std::size_t>(worlds.size(), MAX_INST);

	for (uint32_t i = 0; i < count; ++i) {
		const uint32_t base = 4u * i;

		DirectX::XMFLOAT4 r0f, r1f, r2f, r3f;
		const DirectX::XMMATRIX W = worlds[i];      // NO transpose
		XMStoreFloat4(&r0f, W.r[0]);                // m00 m01 m02 m03
		XMStoreFloat4(&r1f, W.r[1]);                // m10 m11 m12 m13
		XMStoreFloat4(&r2f, W.r[2]);                // m20 m21 m22 m23
		XMStoreFloat4(&r3f, W.r[3]);                // tx  ty  tz  1

		// Proper 3x4: translation lives in .w of each row (no bit twiddling)
		cb->rows[base + 0] = { r0f.x, r0f.y, r0f.z, r3f.x };  // tx
		cb->rows[base + 1] = { r1f.x, r1f.y, r1f.z, r3f.y };  // ty
		cb->rows[base + 2] = { r2f.x, r2f.y, r2f.z, r3f.z };  // tz
		cb->rows[base + 3] = { 0,0,0,1 };                    // padding (keep stride = 4)
	}



	ctx->Unmap(g_cb1.Get(), 0);

	// Bind at b1
	ID3D11Buffer* b = g_cb1.Get();
	ctx->VSSetConstantBuffers(1, 1, &b);
}

Microsoft::WRL::ComPtr<ID3D11Buffer> g_scopeView_b12;

struct alignas(16) ScopeViewCB12_VS
{
	DirectX::XMFLOAT4X4 world_to_projective;   // c0..c3
	DirectX::XMFLOAT4X4 camera_to_world;       // c4..c7
	DirectX::XMFLOAT4   target;                // c8
	DirectX::XMFLOAT4   view_miscellaneous;    // c9
	DirectX::XMFLOAT4   view_unk20;            // c10
	DirectX::XMFLOAT4X4 camera_to_projective;  // c11..c14
};
static_assert(sizeof(ScopeViewCB12_VS) % 16 == 0, "cb must be 16-byte aligned");

void CreateScopeViewCB12(ID3D11Device* dev)
{
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(ScopeViewCB12_VS);
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	ScopeViewCB12_VS zero{}; // start zeroed
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

	cb->view_miscellaneous = view.view_miscellaneous;               // keep your values
	cb->view_unk20 = { 4.15325f, 1.24929f, -1.49012e-8f, 1.0f };

	ctx->Unmap(g_scopeView_b12.Get(), 0);

	ID3D11Buffer* b = g_scopeView_b12.Get();
	ctx->VSSetConstantBuffers(12, 1, &b);
	ctx->PSSetConstantBuffers(12, 1, &b);
	ctx->GSSetConstantBuffers(12, 1, &b); // if GS uses it
}

// Must match the HLSL constant buffer layout exactly (16-byte alignment)
struct VSConstants
{
	DirectX::XMFLOAT4 meshOffset_meshScale; // xyz = meshOffset, w = meshScale
	DirectX::XMFLOAT4 uvScale_uvOffset;     // x = uvScaleX, y = uvOffX, z = uvOffY, w = maxColorOrClamp
};

inline float float_from_bits(std::uint32_t bits) {
	return std::bit_cast<float>(bits);
}

void Graphics::RenderFrame()
{
	const float clear[4] = { 0,0,0,1 };
	pContext->ClearRenderTargetView(pRenderTargetView.Get(), clear);
	pContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->RSSetState(rasterizerState.Get());
	pContext->OMSetDepthStencilState(depthStencilState.Get(), 0);
	pContext->OMSetBlendState(bsOpaque.Get(), nullptr, 0xFFFFFFFF);


	const XMMATRIX view = camera.GetViewMatrix();
	const XMMATRIX proj = camera.GetProjectionMatrix();
	const float targetW = float(windowWidth);
	const float targetH = float(windowHeight);
	const float farZ = camera.GetFarZ();
	const float isFirstPerson = 0.0f;
	static Microsoft::WRL::ComPtr<ID3D11Buffer> s_vsCB; // b1

	if (!s_vsCB) {
		D3D11_BUFFER_DESC cbDesc{};
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.ByteWidth = sizeof(VSConstants);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

		VSConstants init{}; // zeroed by default
		D3D11_SUBRESOURCE_DATA initData{ &init, 0, 0 };
		pDevice->CreateBuffer(&cbDesc, &initData, s_vsCB.GetAddressOf());
	}

	for (auto& Static : static_objects_to_render)
	{
		for (auto& part : Static.parts)
		{

			ID3D11VertexShader* vs = part.materialRender.vs.GetShader();
			ID3D11PixelShader* ps = part.materialRender.ps.GetShader();
			if (!vs || !ps) continue;

			pContext->VSSetShader(vs, nullptr, 0);
			pContext->PSSetShader(ps, nullptr, 0);

			ID3D11Buffer* vbs[] = {
				Static.buffers[part.buffer_index].vertexBuffer.Get(),
				Static.buffers[part.buffer_index].uvBuffer.Get()
			};
			UINT strides[] = {
				Static.buffers[part.buffer_index].vertexBuffer.header.stride,
				Static.buffers[part.buffer_index].uvBuffer.header.stride
			};
			UINT offsets[] = { 0, 0 };

			pContext->IASetInputLayout(part.inputLayout.Get());
			pContext->IASetVertexBuffers(0, 2, vbs, strides, offsets);
			pContext->IASetIndexBuffer(
				Static.buffers[part.buffer_index].indexBuffer.Get(),
				DXGI_FORMAT_R16_UINT, 0);

			VSConstants vsData{};
			vsData.meshOffset_meshScale = { part.mesh_offset[0], part.mesh_offset[1], part.mesh_offset[2], part.mesh_scale };
			vsData.uvScale_uvOffset = { part.texture_coordinate_scale, part.texture_coordinate_offset[0], part.texture_coordinate_offset[1], float_from_bits(part.max_colour_index)  };

			D3D11_MAPPED_SUBRESOURCE m{};
			pContext->Map(s_vsCB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m);
			memcpy(m.pData, &vsData, sizeof(vsData));
			pContext->Unmap(s_vsCB.Get(), 0);

			std::vector<DirectX::XMMATRIX> worlds;
			worlds.reserve(1);

			worlds.push_back(DirectX::XMMatrixIdentity());

			UpdateCB1(
				pContext.Get(),
				/*meshOffset*/{ part.mesh_offset[0], part.mesh_offset[1], part.mesh_offset[2] },
				/*meshScale*/  part.mesh_scale,
				/*uvScaleX*/   part.texture_coordinate_scale,
				/*uvOffX*/     part.texture_coordinate_offset[0],
				/*uvOffY*/     part.texture_coordinate_offset[1],
				/*maxColorOrClampBits*/ (std::uint32_t)part.max_colour_index, // or the exact u32 you need
				worlds
			);

			View viewState_ps{};

			// fill required fields
			XMStoreFloat4x4(&viewState_ps.world_to_camera, camera.GetViewMatrix());
			XMStoreFloat4x4(&viewState_ps.camera_to_projective, camera.GetProjectionMatrix());

			// derive the rest (use your C++ port of derive_matrices or do it inline)
			viewState_ps.derive_matrices_vs(Viewport{ { (float)windowWidth, (float)windowHeight } });

			// now upload
			UploadScopeViewCB12_All(
				pContext.Get(),
				viewState_ps,                 // <-- View struct, not XMMATRIX
				(float)windowWidth,
				(float)windowHeight
			);

			ID3D11Buffer* b1 = g_cb1.Get();
			pContext->VSSetConstantBuffers(1, 1, &b1);

			if (part.material.Unk08[1] == 2)
			{
				ID3D11ShaderResourceView* colSrvs[] = { Static.buffers[part.buffer_index].vertexColourBuffer.GetSRV()};
				pContext->VSSetShaderResources(0, 1, colSrvs);
			}

			if (!part.materialRender.ps_textures.empty()) {
				std::vector<ID3D11ShaderResourceView*> srvs;
				srvs.reserve(part.materialRender.ps_textures.size());
				for (auto& t : part.materialRender.ps_textures) srvs.push_back(t.Get());
				pContext->PSSetShaderResources(0, (UINT)srvs.size(), srvs.data());
			}
			if (part.material.PixelShader.contstant_buffer.hash != 0xffffffff) {
				ID3D11Buffer* psCB = part.materialRender.cbuffer_ps.Get();
				pContext->PSSetConstantBuffers(part.material.PixelShader.constant_buffer_slot, 1, &psCB);
			}
			else {
				ID3D11Buffer* psCB = part.materialRender.cbuffer_ps_fallback.Get();
				pContext->PSSetConstantBuffers(part.material.PixelShader.constant_buffer_slot, 1, &psCB);
			}
			for (auto& samp : part.materialRender.MatSamplers) {
				ID3D11SamplerState* s = samp.sampler.Get();
				pContext->PSSetSamplers((UINT)samp.slot, 1, &s);
			}
			
			pContext->DrawIndexedInstanced(
				part.index_count,
				1,            // InstanceCount
				part.index_start,
				0,
				0);
		}
	}

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
	std::string CameraPrint = std::format("X: {:.2f}  Y: {:.2f}  Z: {:.2f}",
		CameraPos.x, CameraPos.y, CameraPos.z);

	spriteBatch->Begin();
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(),
		DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(),
		DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteBatch->End();

	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test");
	static float value = 50.0f;
	if (ImGui::DragFloat("Speed X:", &value, 1, 0.0f, 100.0f, "%.0f%%")) {
		camera.SetSpeed(value / 10.0f);
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

	StaticRenderer static_loader;
	if (static_loader.Initialize(0x80FB86F3)) {//5482E880
		static_loader.Process();
		this->static_objects_to_render.push_back(static_loader);
	}
	//OutputDebugStringA("Statics initialized.\n");
	if (!InitializeShaders())
		return false;
	OutputDebugStringA("Shaders initialized.\n");

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

		COM_ERROR_IF_FAILED(hr, "Failed to create device and swap chain.");

		Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
		hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
		
		COM_ERROR_IF_FAILED(hr, "Failed to get back buffer.");

		hr = this->pDevice->CreateRenderTargetView(pBackBuffer.Get(), NULL, pRenderTargetView.GetAddressOf());
		
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
		rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_BACK;
		rasterizerDesc.FrontCounterClockwise = FALSE;

		hr = this->pDevice->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());

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
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}
	// Create the camera constant buffer
	CreateScopeViewCB12(pDevice.Get());
	CreateCB1(pDevice.Get());
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

	for (auto& static_obj : this->static_objects_to_render)
	{
		//todo: select layout based on model
	}


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
		HRESULT hr = this->constantBuffer.Initialize(this->pDevice.Get(), this->pContext.Get());
		COM_ERROR_IF_FAILED(hr, "Failed to initialize constant buffer");

		camera.SetPosition(3.0f, 0.0f, 0.0f);
		camera.SetProjectionValues(90.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.01f, 1000.0f);

		for (auto& Static : this->static_objects_to_render)
		{
			Static.InitializeRender(pDevice.Get(), pContext.Get());

		}
	}
	catch (COMException& exception) 
	{
		ErrorLogger::Log(exception);
		return false;
	}
	return true;
}