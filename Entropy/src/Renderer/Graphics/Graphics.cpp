#include "Graphics.h"
#include <filesystem>

bool Graphics::Initialize(HWND hWnd, int width, int height)
{
	this->windowWidth = width;
	this->windowHeight = height;

	if (!InitializeDirectX(hWnd))
		return false;
	OutputDebugStringA("DirectX initialized.\n");

	StaticRenderer static_loader;
	if (static_loader.Initialize(0x81029E6E)) {
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

void Graphics::RenderFrame()
{
	const float clear[4] = { 0, 0, 0, 1 };

	// Clear & fixed pipeline state
	pContext->ClearRenderTargetView(pRenderTargetView.Get(), clear);
	pContext->ClearDepthStencilView(depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->RSSetState(rasterizerState.Get());
	pContext->OMSetDepthStencilState(depthStencilState.Get(), 0);

	// ---- 1) UPDATE + BIND VS CONSTANT BUFFER (b0) BEFORE ANY DRAWS ----
	{
		using namespace DirectX;
		XMMATRIX world = XMMatrixIdentity();
		constantBuffer.data.mat = XMMatrixTranspose(world * camera.GetViewMatrix() * camera.GetProjectionMatrix());
		if (!constantBuffer.ApplyChanges()) return;
		ID3D11Buffer* vsCB = constantBuffer.Get();
		pContext->VSSetConstantBuffers(0, 1, &vsCB); // VS b0
	}

	// Bind a default sampler (to s0; mirror to s1 if your PS expects s1)
	{
		ID3D11SamplerState* s = samplerState.Get();
		pContext->PSSetSamplers(0, 1, &s); // s0
		// If your PS uses register(s1), also bind to slot 1:
		// pContext->PSSetSamplers(1, 1, &s);
	}

	// ---- 2) DRAW STATIC OBJECTS ----
	for (auto& Static : static_objects_to_render)
	{
		for (auto& part : Static.parts)
		{
			// Shaders
			auto* vs = part.materialRender.vs.GetShader();
			auto* ps = part.materialRender.ps.GetShader();
			if (!vs) { OutputDebugStringA("VS is null\n"); continue; }
			if (!ps) { OutputDebugStringA("PS is null\n"); continue; }
			pContext->VSSetShader(vs, nullptr, 0);
			pContext->PSSetShader(ps, nullptr, 0);

			// Per-material PS constant buffer (assume PS cbuffer is b0)
			if (part.materialRender.cbuffer_ps)
			{
				ID3D11Buffer* psCB = part.materialRender.cbuffer_ps.Get();
				pContext->PSSetConstantBuffers(part.material.PixelShader.constant_buffer_slot, 1, &psCB); // PS b0 (NOT 1)
			}

			// PS SRVs (assume shader uses t0..tN)
			std::vector<ID3D11ShaderResourceView*> rawSrvs;
			rawSrvs.reserve(part.materialRender.ps_textures.size());
			for (auto& s : part.materialRender.ps_textures)
				rawSrvs.push_back(s.Get());
			if (!rawSrvs.empty())
				pContext->PSSetShaderResources(0, (UINT)rawSrvs.size(), rawSrvs.data()); // t0+

			// Input Assembler: layout + vertex/index buffers
			pContext->IASetInputLayout(part.inputLayout.Get());

			ID3D11Buffer* vbs[] = {
				Static.buffers[part.buffer_index].vertexBuffer.Get(), // slot 0
				Static.buffers[part.buffer_index].uvBuffer.Get()      // slot 1
			};
			UINT strides[] = {
				Static.buffers[part.buffer_index].vertexBuffer.header.stride,
				Static.buffers[part.buffer_index].uvBuffer.header.stride
			};
			UINT offsets[] = { 0, 0 };
			pContext->IASetVertexBuffers(0, 2, vbs, strides, offsets);
			pContext->IASetIndexBuffer(Static.buffers[part.buffer_index].indexBuffer.Get(), DXGI_FORMAT_R16_UINT, 0);

			// Draw
			using namespace DirectX;
			XMMATRIX world = XMMatrixIdentity();
			XMMATRIX wvp = XMMatrixTranspose(world * camera.GetViewMatrix() * camera.GetProjectionMatrix());
			constantBuffer.data.mat = wvp;
			if (!constantBuffer.ApplyChanges()) return;

			ID3D11Buffer* vsCB = constantBuffer.Get();
			pContext->VSSetConstantBuffers(0, 1, &vsCB);       // VS b0  <-- before any Draw()
			pContext->DrawIndexed(part.index_count, part.index_start, 0);
		}
	}

	// HUD/Text (after your 3D draws)
	if (!fpsTimer.isrunning) fpsTimer.Start();
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

	spriteBatch->Begin();
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(),
		DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(),
		DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteBatch->End();

	// ImGui
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("Test");
	static float value = 50.0f;
	if (ImGui::DragFloat("Speed X:", &value, 1, 0.0f, 100.0f, "%.0f%%"))
		camera.SetSpeed(value / 10.0f);
	ImGui::End();
	ImGui::Render();
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

	pSwapChain->Present(1, 0);
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
		rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;

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

		COM_ERROR_IF_FAILED(hr, "Failed to create device sampler state.");
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}

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

		//std::filesystem::path tex_path = std::filesystem::path(SOLUTION_DIR) / "Data" / "Textures" / "myTex.png";
		//HRESULT hr = DirectX::CreateWICTextureFromFile(
		//	this->pDevice.Get(),
		//	tex_path.c_str(),
		//	nullptr,
		//	this->myTexture.GetAddressOf()
		//);

		//COM_ERROR_IF_FAILED(hr, "Failed to Load Texture");

		HRESULT hr = this->constantBuffer.Initialize(this->pDevice.Get(), this->pContext.Get());
		COM_ERROR_IF_FAILED(hr, "Failed to initialize constant buffer");

		camera.SetPosition(0.0f, 0.0f, -2.0f);
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