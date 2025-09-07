#include "Graphics.h"
#include <filesystem>

bool Graphics::Initialize(HWND hWnd, int width, int height)
{
	this->windowWidth = width;
	this->windowHeight = height;

	if (!InitializeDirectX(hWnd))
		return false;
	OutputDebugStringA("DirectX initialized.\n");

	//StaticRenderer static_loader;
	//if (static_loader.Initialize(0x81029E6E)) {
	//	static_loader.Process();
	//	this->static_objects_to_render.push_back(static_loader);
	//}
	//OutputDebugStringA("Statics initialized.\n");
	if (!InitializeShaders())
		return false;
	OutputDebugStringA("Shaders initialized.\n");

	if (!InitializeSceneOld())
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
	const float color[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

	this->pContext->ClearRenderTargetView(this->pRenderTargetView.Get(), color);
	pContext->ClearDepthStencilView(this->depthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	pContext->IASetInputLayout(this->vertexshader.GetInputLayout());
	pContext->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY::D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	pContext->RSSetState(this->rasterizerState.Get());
	pContext->OMSetDepthStencilState(this->depthStencilState.Get(), 0);
	pContext->PSSetSamplers(0, 1, this->samplerState.GetAddressOf());
	pContext->PSSetShader(this->pixelshader.GetShader(), NULL, 0);
	pContext->VSSetShader(this->vertexshader.GetShader(), NULL, 0);

	UINT offset = 0;

	//Update Constant Buffer
	DirectX::XMMATRIX world = DirectX::XMMatrixIdentity();

	constantBuffer.data.mat = world * camera.GetViewMatrix() * camera.GetProjectionMatrix();
	constantBuffer.data.mat = DirectX::XMMatrixTranspose(constantBuffer.data.mat);
	
	if (!constantBuffer.ApplyChanges()) {
		return;
	}
	this->pContext->VSSetConstantBuffers(0, 1, this->constantBuffer.GetAddressOf());
	//DirectX::XMMATRIX worldMatrix = DirectX::XMMatrixIdentity();

	pContext->PSSetShaderResources(0, 1, this->myTexture.GetAddressOf());
	pContext->IASetVertexBuffers(0, 1, vertexBuffer.GetAddressOf(), vertexBuffer.StridePtr(), &offset);
	pContext->IASetIndexBuffer(indicesBuffer.Get(), DXGI_FORMAT::DXGI_FORMAT_R32_UINT, 0);
	pContext->DrawIndexed(indicesBuffer.BufferSize(), 0, 0);

	//Text
	if (!fpsTimer.isrunning) 
	{
		fpsTimer.Start();
	}
	static int fpsCounter = 0;
	static std::string fpsString = "FPS: 0";
	fpsCounter++;
	if (fpsTimer.GetMilisecondsElapsed() > 1000)
	{
		fpsString = "FPS: " + std::to_string(fpsCounter);
		fpsCounter = 0;
		fpsTimer.Restart();
	}
	auto CameraPos = camera.GetPositionFloat3();
	std::string CameraPrint = std::format("X: {:.2f}  Y: {:.2f}  Z: {:.2f}",
		CameraPos.x, CameraPos.y, CameraPos.z);
	spriteBatch->Begin();
	spriteFont->DrawString(spriteBatch.get(),StringConverter::StringToWide(fpsString).c_str(), DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat, 0.0f, DirectX::XMFLOAT2(0, 0), DirectX::XMFLOAT2(1.0f, 1.0f));
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(), DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat, 0.0f, DirectX::XMFLOAT2(0, 0), DirectX::XMFLOAT2(1.0f, 1.0f));
	spriteBatch->End();




	//IMGUI
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGui::Begin("Test");
	ImGui::End();

	ImGui::Render();

	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());


	pSwapChain->Present(0, NULL);

}

bool Graphics::InitializeDirectX(HWND hWnd)
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
	DXGI_SWAP_CHAIN_DESC scd;
	ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));
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
	
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create device and swap chain.");
		return false;
	}

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
	hr = pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<void**>(pBackBuffer.GetAddressOf()));
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to get back buffer.");
		return false;
	}

	hr = this->pDevice->CreateRenderTargetView(pBackBuffer.Get(), NULL, pRenderTargetView.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create RTV");
		return false;
	}

	D3D11_TEXTURE2D_DESC depthStencilDesc;
	depthStencilDesc.Width = this->windowWidth;
	depthStencilDesc.Height = this->windowHeight;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.ArraySize = 1;
	depthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
	depthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthStencilDesc.CPUAccessFlags = 0;
	depthStencilDesc.MiscFlags = 0;

	hr = this->pDevice->CreateTexture2D(&depthStencilDesc, NULL, this->depthStencilBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil buffer.");
		return false;
	}

	hr = this->pDevice->CreateDepthStencilView(this->depthStencilBuffer.Get(), NULL, this->depthStencilView.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil view.");
		return false;
	}

	this->pContext->OMSetRenderTargets(1, pRenderTargetView.GetAddressOf(), this->depthStencilView.Get());

	D3D11_DEPTH_STENCIL_DESC depthstencildesc;
	ZeroMemory(&depthstencildesc, sizeof(depthstencildesc));
	depthstencildesc.DepthEnable = TRUE;
	depthstencildesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK::D3D11_DEPTH_WRITE_MASK_ALL;
	depthstencildesc.DepthFunc = D3D11_COMPARISON_FUNC::D3D11_COMPARISON_LESS_EQUAL;

	hr = this->pDevice->CreateDepthStencilState(&depthstencildesc, this->depthStencilState.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create depth stencil state.");
		return false;
	}


	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));
	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = float(this->windowWidth);
	viewport.Height = float(this->windowHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	pContext->RSSetViewports(1, &viewport);

	D3D11_RASTERIZER_DESC rasterizerDesc;
	ZeroMemory(&rasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

	rasterizerDesc.FillMode = D3D11_FILL_MODE::D3D11_FILL_SOLID;
	rasterizerDesc.CullMode = D3D11_CULL_MODE::D3D11_CULL_NONE;
	//rasterizerDesc.FrontCounterClockwise = FALSE;
	hr = this->pDevice->CreateRasterizerState(&rasterizerDesc, this->rasterizerState.GetAddressOf());
	if (hr != S_OK)
	{
		ErrorLogger::Log(hr, "Failed to create rasterizer state.");
		return false;
	}
	std::filesystem::path font_path = std::filesystem::path(SOLUTION_DIR) / "Data" / "Fonts" / "entropy.spritefont";
	spriteBatch = std::make_unique<DirectX::SpriteBatch>(this->pContext.Get());
	spriteFont = std::make_unique<DirectX::SpriteFont>(
		this->pDevice.Get(),
		font_path.c_str());

	D3D11_SAMPLER_DESC samplerDesc;
	ZeroMemory(&samplerDesc, sizeof(samplerDesc));
	samplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	samplerDesc.MinLOD = 0;
	samplerDesc.MaxLOD = D3D11_FLOAT32_MAX;
	samplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;

	hr = this->pDevice->CreateSamplerState(&samplerDesc, this->samplerState.GetAddressOf());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create sampler state.");
		return false;
	}

	return true;
}

bool Graphics::InitializeShaders()
{
	D3D11_INPUT_ELEMENT_DESC layout[] =
	{
		{"POSITION", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT::DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0},

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

	for (auto& static_obj : this->static_objects_to_render)
	{
		//Load Index Buffers
		for (auto &buffer_group : static_obj.buffers) {
			D3D11_BUFFER_DESC vertexBufferDesc;
			ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

			vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			vertexBufferDesc.ByteWidth = buffer_group.vertexBuffer.header.dataSize;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vertexBufferDesc.CPUAccessFlags = 0;
			vertexBufferDesc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA vertexBufferData;
			ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
			vertexBufferData.pSysMem = buffer_group.vertexBuffer.data;

			//HRESULT hr = this->pDevice->CreateBuffer(&vertexBufferDesc, &vertexBufferData, vertexBuffer.GetAddressOf());

		}
	}
	return true;
	

};


bool Graphics::InitializeSceneOld()
{
	Vertex v[] = 
	{
		Vertex(-0.5f, -0.5f, 0.0f, 0.0f, 1.0f),//BL 0
		Vertex(-0.5f, 0.5f, 0.0f, 0.0f, 0.0f),//TL  1
		Vertex(0.5f, 0.5f, 0.0f, 1.0f, 0.0f),//TR   2
		Vertex(0.5f, -0.5f, 0.0f, 1.0f, 1.0f)//BR   3

	};

	DWORD indices[] = 
	{
		0,1,2,
		0,2,3
	};



	HRESULT hr = this->vertexBuffer.Initialize(pDevice.Get(), v, ARRAYSIZE(v));

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create vertex buffer.");
		return false;
	}

	//Load Index Buffer

	hr = indicesBuffer.Initialize(pDevice.Get(), indices, ARRAYSIZE(indices));

	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create index buffer.");
		return false;
	}

	std::filesystem::path tex_path = std::filesystem::path(SOLUTION_DIR) / "Data" / "Textures" / "myTex.png";
	hr = DirectX::CreateWICTextureFromFile(
		this->pDevice.Get(),
		tex_path.c_str(),
		nullptr,
		this->myTexture.GetAddressOf()
	);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to load texture.");
		return false;
	}


	hr = this->constantBuffer.Initialize(this->pDevice.Get(), this->pContext.Get());
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to initialize constant buffer.");
		return false;
	}


	camera.SetPosition(0.0f, 0.0f, -2.0f);
	camera.SetProjectionValues(90.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.01f, 1000.0f);

	return true;
}