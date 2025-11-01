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
#include "TigerEngine/globaldata.h"
#include <dxgi.h>    
#include <dxgiformat.h> 
#include "Renderer/Graphics/Render/gbuffer.h"
#include "GameTimer.h"
#include "TigerEngine/Technique/rasterizer_states.h"
#include "RenderStates.h"
#include "Renderer/Loaders/Scope.h"
#include "TigerEngine/Technique/Tfx/global_channels.h"

enum class PipelineStage { Forward, GBuffer, Lighting };


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
	bool InitializeRenderGlobals();
	void InitializeInputLayouts();
	void InitAnnotation();
	void DrawStaticSpecial(const RenderStatic& rs, const View& view);

	std::array<Microsoft::WRL::ComPtr<ID3D11InputLayout>, 15> tiger_input_layouts;

	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11BlendState> bsOpaque;

	PixelShader pixelshader;
	VertexShader vertexshader;

	UINT offset = 0;

	GameTimer gTimer;

	ExternStorage externs = ExternStorage::FilledDefaults();

	Model model;

	CD3D11_VIEWPORT viewport;

	std::vector<std::pair<std::string, TigerScope>> scopes;

	ComPtr<ID3D11DepthStencilState> dsWriteG;     // NEW (GREATER)
	ComPtr<ID3D11DepthStencilState> dsReadOnlyG;  // NEW (GREATER, write=ZERO)

	Microsoft::WRL::ComPtr<ID3D11DepthStencilView> depthStencilView;
	Microsoft::WRL::ComPtr<ID3D11Texture2D> depthStencilBuffer;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilState;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilDecal;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilStateLighting;

	Microsoft::WRL::ComPtr <ID3D11DepthStencilState> dsDisabled;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerState;
	Microsoft::WRL::ComPtr<ID3D11BlendState> blendState;

	std::unique_ptr<DirectX::SpriteBatch> spriteBatch;
	std::unique_ptr<DirectX::SpriteFont> spriteFont;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> samplerState;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> myTexture;
	Microsoft::WRL::ComPtr <ID3D11Buffer> cbCombine;
	Microsoft::WRL::ComPtr <ID3D11Buffer> cbGlobalAmbient;

	ComPtr<ID3D11SamplerState> samplerPointClamp;
	ComPtr<ID3D11SamplerState> samplerLinearClamp;
	ComPtr<ID3D11BlendState> bsMultiply;

	ComPtr<ID3D11SamplerState> shading1;
	ComPtr<ID3D11SamplerState> shading2;
	ComPtr<ID3D11SamplerState> lighting1;
	ComPtr<ID3D11SamplerState> lighting2;


	Microsoft::WRL::ComPtr<ID3D11BlendState> bsGBufferOpaqueIndependent;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateGBuffer;

	RenderStates states;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> dbgFullscreenVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  dbgDeferredPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  dbgCopyPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  psSolid;

	Microsoft::WRL::ComPtr<ID3D11BlendState> bsAdditive; // optional, for later draws

	int windowWidth = 0;
	int windowHeight = 0;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler;

	PipelineStage pipelineStage = PipelineStage::Forward;

	Timer fpsTimer;

	std::vector<RenderStatic> staticsToDraw;

	void DrawStaticMesh(const RenderStatic& rs, const View& view);
	void CreateWhite1x1SRV();
	void CreateCB13();
	void InitializeScopes();
	void LoadGlobalTextures();

	std::array< GlobalChannel,256> channels = GetGlobalChannelDefaults();

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> global_textures;

	std::unordered_map<std::string, std::shared_future<std::shared_ptr<EntropyAssets::Technique>>> globalTechniques;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> fsTriVS; // fullscreen triangle VS
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  gbufferPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  deferredPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       deferredCamCB; // InvProj + CameraPos
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateNoCull;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> white1x1SRV;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cb13_;

	GBuffer gbufA;
};