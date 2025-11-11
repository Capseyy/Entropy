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
//#include "Renderer/Graphics/Render/gbuffer.h"
#include "GameTimer.h"
#include "TigerEngine/Technique/rasterizer_states.h"
#include "RenderStates.h"
#include "Renderer/Loaders/Scope.h"
#include "TigerEngine/Technique/Tfx/global_channels.h"
#include <wincodec.h>
#include "Renderer/Loaders/TextureLoader.h"
#include "Renderer/Loaders/Map.h"
#include "Renderer/Graphics/Render/GBufferRT.h"
#include "Render/FrustumCulling.h"
#include "Render/RenderPacket.h"
#include <atomic>
#include <mutex>

enum class StaticBufKind { Index, Vertex, UV, Color };

enum class TfxRenderStage : uint8_t {
	GenerateGbuffer = 0,
	Decals = 1,
	InvestmentDecals = 2,
	ShadowGenerate = 3,
	LightingApply = 4,
	LightProbeApply = 5,
	DecalsAdditive = 6,
	Transparents = 7,
	Distortion = 8,
	LightShaftOcclusion = 9,
	SkinPrepass = 10,
	LensFlares = 11,
	DepthPrepass = 12,
	WaterReflection = 13,
	PostprocessTransparentStencil = 14,
	Impulse = 15,
	Reticle = 16,
	WaterRipples = 17,
	MaskSunLight = 18,
	Volumetrics = 19,
	Cubemaps = 20,
	PostprocessScreen = 21,
	WorldForces = 22,
	ComputeSkinning = 23,
};


struct ResolvedSpecial {
	std::shared_ptr<ID3D11Buffer> ib;
	std::shared_ptr<ID3D11Buffer> vb1;
	std::shared_ptr<ID3D11Buffer> vb2;
	std::shared_ptr<EntropyAssets::BufferSRVRes> vCol;

	UINT   stride1 = 0;
	UINT   stride2 = 0;
	DXGI_FORMAT idxFmt = DXGI_FORMAT_R16_UINT;
	DXGI_FORMAT vColFmt = DXGI_FORMAT_UNKNOWN;

	uint32_t indexStart = 0;
	uint32_t indexCount = 0;

	bool ready = false;
};

enum class SpecialBufKind { Index, VB1, VB2, Color };

// Graphics.h (or a nearby header)
struct ResolvedStaticPart
{
	std::shared_ptr<ID3D11Buffer> ib;
	std::shared_ptr<ID3D11Buffer> vb0;
	std::shared_ptr<ID3D11Buffer> vb1;    // optional UV
	std::shared_ptr<EntropyAssets::BufferSRVRes> vCol; // optional color SRV
	DXGI_FORMAT vCOlfmt = DXGI_FORMAT_R8G8B8A8_UNORM;
	UINT stride0 = 0, stride1 = 0;
	DXGI_FORMAT idxFmt = DXGI_FORMAT_R16_UINT;
	bool ready = false;
	uint32_t indexCount = 0;
	uint32_t indexStart = 0;
	uint8_t  lastIL = 0xFF;               // to reduce IL rebinds
};

struct InstanceData
{
	DirectX::XMFLOAT4 translation; // xyz + maybe w (leave as 4 floats for alignment)
	DirectX::XMFLOAT4 rotation;    // quaternion
	float              scale;      // 1 float
	float              _pad[3];    // pad to 16B multiple (stride = 48 bytes)
};

class Graphics
{
public:
	bool Initialize(HWND hWnd, int width, int height);
	void RenderFrame();

	Camera camera;
	std::unique_ptr<LoadZone> loadzone;
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
	void DrawStaticTransparent(const RenderStatic& rs, const View& view);
	void DrawEntity(const RenderEntity& rs, const View& view, TfxRenderStage = TfxRenderStage::GenerateGbuffer);
	void RunPostprocessChain();
	//Asset Cache
	std::unordered_map<uint32_t, std::shared_future<std::shared_ptr<ID3D11Buffer>>> bufferFut_;      // VB/UV/IB
	std::unordered_map<uint32_t, std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>> bufferSrvFut_; // color SRV
	std::shared_ptr<EntropyAssets::Technique> GetStaticTechniqueOrEnqueue(uint32_t techId);

	std::mutex bufferCacheMutex_;
	std::mutex bufferSrvCacheMutex_;
	void EnsureSpecialBufferRegistered(const SStaticSpecial& sp,
		StaticBufKind which,
		UINT addFlags);

	// build helpers
	void DrawStaticMeshTo(std::vector<DrawPacket>& outPackets,
		std::vector<ObjectVectors>& outWorlds,
		std::vector<InstanceData>& outInstances,
		const RenderStatic& rs,
		const View& view,
		TfxRenderStage renderStage);
	void DrawStaticMeshParallel(const std::vector<RenderStatic>& staticsToDraw, const View& view, TfxRenderStage renderStage);

	std::unordered_map<uint64_t, ResolvedSpecial> specialsCache_;
	std::array<Microsoft::WRL::ComPtr<ID3D11InputLayout>, 15> tiger_input_layouts;
	std::vector<DrawPacket> packets_;
	Microsoft::WRL::ComPtr<ID3D11Device>           pDevice;
	Microsoft::WRL::ComPtr<ID3D11DeviceContext>    pContext;
	Microsoft::WRL::ComPtr<IDXGISwapChain>         pSwapChain;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetView;
	Microsoft::WRL::ComPtr<ID3D11BlendState> bsOpaque;
	std::vector<ObjectVectors> frameWorlds_;
	void EnsureStaticBufferRegistered(const SStaticMeshData& mesh,
		const SStaticMeshPart& part,
		StaticBufKind which,
		UINT addFlags);
	uint8_t* m_instWritePtr = nullptr;
	std::atomic<UINT> m_instCursor{ 0 };   // in elements (not bytes)
	std::atomic<UINT> m_worldCursor{ 0 };
	bool ResolveStaticPartOnce(
		const SStaticMeshData& mesh,
		const SStaticMeshPart& part,
		ResolvedStaticPart& out);
	std::unordered_map<uint64_t, ResolvedStaticPart> staticPartCache_;
	void EnsureBufferBind(uint32_t id, UINT addFlags);
	std::shared_future<std::shared_ptr<ID3D11Buffer>>&
		GetOrEnqueueBuffer(uint32_t id, UINT addFlags);
	std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>&
		GetOrEnqueueBufferSRV(uint32_t id);
	std::unordered_map<uint32_t,
		std::shared_future<std::shared_ptr<EntropyAssets::Technique>>> TechCache_;
	void SubmitPackets(ID3D11DeviceContext* ctx, std::vector<DrawPacket>& packets, TfxRenderStage stage);
	bool ResolveSpecialOnce(const SStaticSpecial& sp, ResolvedSpecial& out);

	Microsoft::WRL::ComPtr<ID3D11Buffer>              m_instanceSB;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>  m_instanceSRV;
	UINT  m_instanceStride = sizeof(InstanceData);
	UINT  m_instanceCapacity = 200000;
	void CreateInstanceBuffer();
	Microsoft::WRL::ComPtr<ID3D11VertexShader> entity_vs_override;
	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> dsDepthReadWrite_;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState>   rsNoCull_;
	void DrawPostProcessPass(bool enableFXAA, bool fxaaNoise /*unused here*/);
	void DrawFS(ID3D11DeviceContext* ctx,
		ID3D11VertexShader* vs, ID3D11PixelShader* ps,
		ID3D11RenderTargetView* rtv,
		ID3D11ShaderResourceView* const* srvs, UINT srvCount,
		ID3D11SamplerState* const* samps, UINT sampCount);
	void IssueOcclusionQueries(const View& view);

	UINT offset = 0;

	GameTimer gTimer;

	ExternStorage externs = ExternStorage::FilledDefaults();

	Model model;

	CD3D11_VIEWPORT viewport;
	ComPtr<ID3D11DepthStencilState> dsLightRO_Greater;

	ComPtr<ID3D11RasterizerState> rsCullFront, rsCullBack;

	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerCullFront;

	std::vector<std::pair<std::string, TigerScope>> scopes;

	Microsoft::WRL::ComPtr<ID3D11Texture2D> pBackBuffer;
	Microsoft::WRL::ComPtr<ID3D11RenderTargetView> pRenderTargetViewLinear;

	MapStaticAO staticAO1;

	Microsoft::WRL::ComPtr<ID3D11DepthStencilState> depthStencilLightVolume;

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

	Microsoft::WRL::ComPtr<ID3D11Buffer> g_cb1_fallback;

	Microsoft::WRL::ComPtr<ID3D11BlendState> bsGBufferOpaqueIndependent;
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateGBuffer;

	RenderStates states;

	bool LoadGlobalTextureOptional(const std::string& key,const std::filesystem::path& onDiskPath,int resourceId, bool forceSRGB);

	Microsoft::WRL::ComPtr<ID3D11VertexShader> dbgFullscreenVS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  dbgDeferredPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  dbgCopyPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  psSolid;

	Microsoft::WRL::ComPtr<ID3D11BlendState> bsAdditive; // optional, for later draws

	int windowWidth = 0;
	int windowHeight = 0;

	Microsoft::WRL::ComPtr<ID3D11SamplerState> pointSampler;

	Timer fpsTimer;

	Microsoft::WRL::ComPtr<ID3D11Buffer> lightCubeVB;
	Microsoft::WRL::ComPtr<ID3D11Buffer> lightCubeIB;
	UINT lightCubeIndexCount = 0;

	std::vector<RenderStatic> staticsToDraw;
	std::vector<RenderLight> lightsToDraw;
	std::vector<RenderEntity> entitiesToDraw;

	Microsoft::WRL::ComPtr<ID3D11BlendState> bsAdditive2RT;

	void DrawStaticMesh(const RenderStatic& rs, const View& view, TfxRenderStage renderStage);
	void Create1x1SRV(UINT color, ComPtr<ID3D11ShaderResourceView>&);
	void InitializeScopes();
	void LoadGlobalTextures();
	void DrawLight(const RenderLight& rs, const View& view);
	void CreateLightVolumeResources();


	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateBiased;
	std::array< GlobalChannel,256> channels = GetGlobalChannelDefaults();

	std::unordered_map<std::string, Microsoft::WRL::ComPtr<ID3D11ShaderResourceView>> global_textures;

	std::unordered_map<std::string, std::shared_future<std::shared_ptr<EntropyAssets::Technique>>> globalTechniques;

	Microsoft::WRL::ComPtr<ID3D11VertexShader> fsTriVS; // fullscreen triangle VS
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  gbufferPS;
	Microsoft::WRL::ComPtr<ID3D11PixelShader>  deferredPS;
	Microsoft::WRL::ComPtr<ID3D11Buffer>       deferredCamCB; // InvProj + CameraPos
	Microsoft::WRL::ComPtr<ID3D11RasterizerState> rasterizerStateNoCull;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> white1x1SRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> grey1x1SRV;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> temp_angle_lookup;
	Microsoft::WRL::ComPtr<ID3D11Buffer> cb13_;

	GBufferRT gbufA;
};