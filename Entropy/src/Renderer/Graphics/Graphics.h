#pragma once
#include <Renderer/Graphics/GPUAdapter.h>
#include "Shaders/Shaders.h"
#include "SpriteBatch.h"
#include "SpriteFont.h"
#include "WICTextureLoader.h"
#include "Renderer/Loaders/StaticMap.h"
#include "Renderer/Graphics/Camera.h"
#include "Renderer/Graphics/Buffers/ConstantBufferTypes.h"
#include "Renderer/Timer.h";
#include "Renderer/Graphics/ImGui/imgui.h"
#include "Renderer/Graphics/ImGui/imgui_impl_win32.h"
#include "Renderer/Graphics/ImGui/imgui_impl_dx11.h"
#include "Renderer/Graphics/Scope/view.h"
#include "Runtime/Threading/ThreadPool.h"
#include "Runtime/Threading/MainThreadQueue.h"
#include "Runtime/Assets/AssetSystem.h"
#include "Runtime/Assets/RuntimeAssetRegistry.h"
#include "Model.h"
#include "TigerEngine/Technique/input_layout.h"
#include "Scope/instance.h"

struct GBuffer {
	UINT w = 0, h = 0;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> albedo, normalRgh, material;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> albedoRTV, normalRghRTV, materialRTV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> albedoSRV, normalRghSRV, materialSRV;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> depth;      // R24G8 typeless
	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> dsv;  // D24S8
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> depthSRV; // R24_UNORM_X8
};

enum class PipelineStage { Forward, GBuffer };

class Graphics
{
public:
	bool Initialize(HWND hWnd, int width, int height);
	void RenderFrame();

	Camera camera;

	std::unique_ptr<StaticMap> staticMap;   // NEW
	std::unique_ptr<ThreadPool>            pool;
	std::unique_ptr<MainThreadQueue>       mainQueue;
	std::unique_ptr<RuntimeAssetRegistry>  registry;
	std::unique_ptr<AssetSystem>           assets;


private:
	bool InitializeDirectX(HWND hWnd);
	bool InitializeShaders();
	bool InitializeScene();
	void InitializeInputLayouts();
	void InitAnnotation();
	std::array<Microsoft::WRL::ComPtr<ID3D11InputLayout>,15> tiger_input_layouts;

	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11BlendState> bsOpaque;

	PixelShader pixelshader;
	VertexShader vertexshader;

	UINT offset = 0;

	Model model;

	CD3D11_VIEWPORT viewport;

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

	std::vector<RenderStatic> staticsToDraw;

	void DrawStaticMesh(const RenderStatic& rs, const View& view);

	//gbuffer
	bool useDeferred = true;
	PipelineStage pipelineStage = PipelineStage::Forward;
	GBuffer gbuf{};

	Microsoft::WRL::ComPtr<ID3D11VertexShader> fsTriVS; // fullscreen triangle VS
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  gbufferPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  deferredPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       deferredCamCB; // InvProj + CameraPos

	// Helpers
	void CreateOrResizeGBuffer(UINT w, UINT h);
	void BindGBufferForWriting();
	void RunDeferredLighting();
};
