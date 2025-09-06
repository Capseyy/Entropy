#pragma once
#include <Renderer/Graphics/GPUAdapter.h>
#include "Shaders/Shaders.h"
#include "Renderer/Graphics/Shaders/Vertex.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "WICTextureLoader.h"
#include "Renderer/Loaders/static_loader.h"

class Graphics
{
public:
	bool Initialize(HWND hWnd, int width, int height);
	void RenderFrame();

private:
	bool InitializeDirectX(HWND hWnd, int width, int height);
	bool InitializeShaders();
	bool InitializeSceneOld();
	bool InitializeScene();

	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;

	PixelShader pixelshader;
	VertexShader vertexshader;

	std::vector<StaticRenderer> static_objects_to_render;

	Microsoft::WRL::ComPtr<ID3D11Buffer> vertexBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> indexBuffer;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

	std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
	std::unique_ptr<DirectX::SpriteFont> spriteFont;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> myTexture;
};
