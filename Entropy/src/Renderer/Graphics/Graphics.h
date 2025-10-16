#pragma once
#include <Renderer/Graphics/GPUAdapter.h>
#include "Shaders/Shaders.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "WICTextureLoader.h"
#include "Renderer/Loaders/static_loader.h"
#include "Renderer/Graphics/Camera.h"
#include "Renderer/Graphics/Buffers/ConstantBufferTypes.h"
#include "Renderer/Timer.h";
#include "Renderer/Graphics/ImGui/imgui.h"
#include "Renderer/Graphics/ImGui/imgui_impl_win32.h"
#include "Renderer/Graphics/ImGui/imgui_impl_dx11.h"
#include "Renderer/Graphics/Scope/view.h"
#include "Model.h"

class Graphics
{
public:
	bool Initialize(HWND hWnd, int width, int height);
	void RenderFrame();

	Camera camera;

private:
	bool InitializeDirectX(HWND hWnd);
	bool InitializeShaders();
	bool InitializeScene();

	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11BlendState> bsOpaque;

	PixelShader pixelshader;
	VertexShader vertexshader;

	std::vector<StaticRenderer> static_objects_to_render;

	UINT offset = 0;

	Model model;

	CD3D11_VIEWPORT viewport;

	ConstantBuffer<CB_VS_vertexshader> constantBuffer;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;

	std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
	std::unique_ptr<DirectX::SpriteFont> spriteFont;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> myTexture;

	int windowWidth = 0;
	int windowHeight = 0;

	Timer fpsTimer;
};
