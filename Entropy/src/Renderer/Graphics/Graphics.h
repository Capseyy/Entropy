#pragma once
#include <Renderer/Graphics/GPUAdapter.h>
#include "Shaders/Shaders.h"
#include "Renderer/Graphics/Shaders/Vertex.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "WICTextureLoader.h"
#include "Renderer/Loaders/static_loader.h"
#include "Renderer/Graphics/Camera.h"
#include "Renderer/Graphics/Buffers/VertexBuffer.h"
#include "Renderer/Graphics/Buffers/IndexBuffer.h"
#include "Renderer/Graphics/Buffers/ConstantBufferTypes.h"

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

	UINT offset = 0;

	VertexBuffer<Vertex> vertexBuffer;
	IndexBuffer indicesBuffer;
	Microsoft::WRL::ComPtr<ID3D11Buffer> constantBuffer;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;

	std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
	std::unique_ptr<DirectX::SpriteFont> spriteFont;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> myTexture;

	int windowWidth = 0;
	int windowHeight = 0;

	Camera camera;
};
