#include "Graphics.h"
#include <filesystem>
#include <d3dcompiler.h>
#include "UI/AtmosphereExternUI.h"
#include "UI/FrameGlobalLightingExternUI.h"
#include "UI/DeferredExternUI.h"
#include "UI/global_channel_ui.h"
#include "UI/ChannelEditor.h"
#include <unordered_set>
#include <cfloat>
#include "Renderer/Graphics/UI/ActivityBrowser.h"
#include "TigerEngine/Map/TigerBuffer.h"
#include "TigerEngine/Technique/Tfx/global_channel_usage.h"
#include "Renderer/Graphics/UI/TfxUI.h"

static int g_activation_budget_per_frame = 8;
static int g_activations_this_frame = 0;


#pragma comment(lib, "d3dcompiler.lib")
#define GLM_FORCE_DEFAULT_ALIGNED_GENTYPES

Microsoft::WRL::ComPtr<ID3DUserDefinedAnnotation> anno_;
void Graphics::InitAnnotation() {
	pContext->QueryInterface(IID_PPV_ARGS(anno_.GetAddressOf())); 
}

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
namespace {
    static inline const char* EntityTypeName(EntityType t) {
        switch (t) {
        case EntityType::Standard: return "Standard";
        case EntityType::Activity: return "Activity";
        case EntityType::ParticleSystem: return "ParticleSystem";
        case EntityType::Combatant: return "Combatant";
        case EntityType::SkyEntity: return "SkyEntity";
        case EntityType::ChildEntity: return "ChildEntity";
        case EntityType::CombatantChild: return "CombatantChild";
        default: return "Unknown";
        }
    }

    
	static inline bool WorldToScreen(const View& view, const glm::vec3& world, float screenW, float screenH, ImVec2& out) {
		using namespace DirectX;

		const XMMATRIX W2P = XMLoadFloat4x4(&view.world_to_projective);

		const XMVECTOR p = XMVectorSet(world.x, world.y, world.z, 1.0f);
		const XMVECTOR clip = XMVector4Transform(p, W2P);

		const float w = XMVectorGetW(clip);
		if (w <= 1e-5f) return false;

		const float invW = 1.0f / w;
		const float ndcX = XMVectorGetX(clip) * invW;
		const float ndcY = XMVectorGetY(clip) * invW;

		
		if (ndcX < -1.0f || ndcX > 1.0f || ndcY < -1.0f || ndcY > 1.0f) return false;

		out.x = (ndcX * 0.5f + 0.5f) * screenW;
		out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * screenH;
		return true;
	
    }
}







template<typename T>
static bool TryActivateReady(std::shared_future<std::shared_ptr<T>>& fut,
	std::shared_ptr<T>& out)
{
	if (!FutReady(fut)) return false;
	if (g_activations_this_frame >= g_activation_budget_per_frame) return false;

	try {
		out = fut.get();            
	}
	catch (const std::exception& e) {
		out.reset();           
		return false;
	}
	++g_activations_this_frame;
	return true;
}

static inline CB1Payload_override BuildCB1FromEntity(const RenderEntity& rs)
{
	using namespace DirectX;

	CB1Payload_override cb{};

	
	const auto& model_offset = rs.meshData.model_offset;  
	const auto& model_scale = rs.meshData.model_scale;   
	const float instance_scale = rs.pos.w;

	const float texScale = rs.meshData.texcoord_scale.x;
	const float texOffX = rs.meshData.texcoord_offset.x;
	const float texOffY = rs.meshData.texcoord_offset.y;

	const glm::quat& rot = rs.rot;
	const glm::vec3  pos(rs.pos.x, rs.pos.y, rs.pos.z);
	const XMVECTOR q = XMVectorSet(rot.w, rot.x, rot.y, rot.z);
	const XMMATRIX R = XMMatrixRotationQuaternion(q);
	const XMMATRIX T = XMMatrixTranslation(pos.x, pos.y, pos.z);

	const float s = instance_scale;
	const XMMATRIX S = XMMatrixScaling(s, s, s);

	const XMMATRIX M = S * R * T;
	XMStoreFloat4x4(&cb.mesh_to_world, M);

	cb.position_scale = XMFLOAT4(model_scale.x, model_scale.y, model_scale.z, model_scale.w);
	cb.position_offset = XMFLOAT4(model_offset.x, model_offset.y, model_offset.z, model_offset.w);

	cb.texcoord0_scale_offset = XMFLOAT4(texScale, texScale, texOffX, texOffY);
	cb.dynamic_sh_ao_values = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);

	return cb;
}

BufferPayload BuildBufferPayloadFromTag(TagHash tag, StaticBufKind which)
{
	BufferPayload p{};
	p.id = tag.hash;

	if (!tag.data || tag.size == 0)
		throw std::runtime_error("BuildBufferPayloadFromTag: header tag is empty");

	if (which == StaticBufKind::Index) {

		const IndexBufferHeader ibh = bin::parse<IndexBufferHeader>(tag.data, tag.size, bin::Endian::Little);

		TagHash dataTag(tag.reference);
		if (!dataTag.data || dataTag.size == 0)
			throw std::runtime_error("BuildBufferPayloadFromTag: index data tag empty");

		const size_t want = static_cast<size_t>(ibh.dataSize);
		if (dataTag.size < want)
			throw std::runtime_error("BuildBufferPayloadFromTag: index data smaller than header declared size");

		p.desc.Usage = D3D11_USAGE_IMMUTABLE;
		p.desc.BindFlags = D3D11_BIND_INDEX_BUFFER;   
		p.desc.ByteWidth = static_cast<UINT>(want);
		p.desc.CPUAccessFlags = 0;
		p.desc.MiscFlags = 0;

		p.stride = (ibh.is32 != 0) ? 4u : 2u;        
		const auto* bytes = static_cast<const uint8_t*>(dataTag.data);
		p.data.assign(bytes, bytes + want);
	}
	else {
		
		const VertexBufferHeader vbh = bin::parse<VertexBufferHeader>(tag.data, tag.size, bin::Endian::Little);

		TagHash dataTag(tag.reference);
		if (!dataTag.data || dataTag.size == 0)
			throw std::runtime_error("BuildBufferPayloadFromTag: vertex data tag empty");

		const size_t want = static_cast<size_t>(vbh.dataSize);
		if (dataTag.size < want)
			throw std::runtime_error("BuildBufferPayloadFromTag: vertex data smaller than header declared size");

		p.desc.Usage = D3D11_USAGE_IMMUTABLE;
		p.desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		if (which == StaticBufKind::Color) {
			
			p.desc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		p.desc.ByteWidth = static_cast<UINT>(want);
		p.desc.CPUAccessFlags = 0;
		p.desc.MiscFlags = 0;

		p.stride = vbh.stride;                       
		const auto* bytes = static_cast<const uint8_t*>(dataTag.data);
		p.data.assign(bytes, bytes + want);
	}
	return p;
}

bool Graphics::ResolveSpecialOnce(const SStaticSpecial& sp, ResolvedSpecial& out)
{
	
	EnsureSpecialBufferRegistered(sp, StaticBufKind::Index, D3D11_BIND_INDEX_BUFFER);
	EnsureSpecialBufferRegistered(sp, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);
	if (sp.VertexBuffer2.hash && sp.VertexBuffer2.hash != 0xFFFFFFFFu)
		EnsureSpecialBufferRegistered(sp, StaticBufKind::UV, D3D11_BIND_VERTEX_BUFFER);
	if (sp.VertexColourBuffer.hash && sp.VertexColourBuffer.hash != 0xFFFFFFFFu)
		EnsureSpecialBufferRegistered(sp, StaticBufKind::Color, D3D11_BIND_SHADER_RESOURCE);

	
	auto& fIB = GetOrEnqueueBuffer(sp.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
	auto& fVB1 = GetOrEnqueueBuffer(sp.VertexBuffer1.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<ID3D11Buffer>>* fVB2 = nullptr;
	if (sp.VertexBuffer2.hash && sp.VertexBuffer2.hash != 0xFFFFFFFFu)
		fVB2 = &GetOrEnqueueBuffer(sp.VertexBuffer2.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>* fCol = nullptr;
	const bool hasColor = (sp.VertexColourBuffer.hash && sp.VertexColourBuffer.hash != 0xFFFFFFFFu);
	if (hasColor) fCol = &GetOrEnqueueBufferSRV(sp.VertexColourBuffer.hash);

	
	if (!FutReady(fIB) || !FutReady(fVB1) ||
		(fVB2 && !FutReady(*fVB2)) ||
		(hasColor && (!fCol || !FutReady(*fCol))))
		return false;

	
	out.ib = fIB.get();
	out.vb1 = fVB1.get();
	out.vb2 = (fVB2 ? fVB2->get() : nullptr);

	out.vCol = nullptr;
	if (hasColor && fCol) {
		try { out.vCol = fCol->get(); }
		catch (...) { out.vCol = nullptr; }
	}

	
	const auto pIB = registry->GetBuffer(sp.IndexBuffer.hash);
	const auto pVB1 = registry->GetBuffer(sp.VertexBuffer1.hash);

	BufferPayload pVB2{};
	if (out.vb2) pVB2 = registry->GetBuffer(sp.VertexBuffer2.hash);

	BufferPayload pVC{};
	if (out.vCol) pVC = registry->GetBuffer(sp.VertexColourBuffer.hash);

	
	out.idxFmt = (pIB.stride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	out.indexStart = sp.index_start;
	out.indexCount = sp.index_count;
	out.stride1 = pVB1.stride;
	out.stride2 = out.vb2 ? pVB2.stride : 0;

	if (out.vCol) {
		out.vColFmt = (pVC.stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	}

	out.ready = true;
	return true;
}

static inline TagHash GetBufTag(const SStaticMeshData& mesh,
	const SStaticMeshPart& part,
	StaticBufKind which)
{
	const auto& B = mesh.buffers[part.buffer_index];
	switch (which) {
	case StaticBufKind::Index: return B.IndexBuffer;
	case StaticBufKind::Vertex: return B.VertexBuffer;
	case StaticBufKind::UV: return B.UVBuffer;
	case StaticBufKind::Color: return B.VertexColourBuffer;
	}
	return {};
}

void Graphics::SubmitPackets(ID3D11DeviceContext* ctx,
	std::vector<DrawPacket>& packets,
	TfxRenderStage stage)
{
	wchar_t label[128];
	_snwprintf_s(label, _TRUNCATE, L"SubmitPackets [%ls]", StageName(stage));
	ScopedGpuEvent _stage(anno_.Get(), label);
	struct Item { uint64_t key; uint32_t low; DrawPacket* p; };
	static std::vector<Item> order;
	order.clear(); order.reserve(packets.size());
	for (auto& d : packets) order.push_back({ MakeStateKey(d), d.sortKeyLow, &d });

	std::sort(order.begin(), order.end(), [stage](const Item& a, const Item& b) {
		if (stage == TfxRenderStage::Transparents) {
			if (a.low != b.low) return a.low > b.low;   
			if (a.key != b.key) return a.key < b.key;
			return a.p->meshId < b.p->meshId;
		}
		else {
			if (a.key != b.key) return a.key < b.key;
			return a.low < b.low;
		}
		});

	uint64_t curKey = ~0ull;
	ID3D11InputLayout* curIL = nullptr;
	ID3D11Buffer* curVBs[2] = {};
	UINT curStrides[2] = {};
	ID3D11Buffer* curIB = nullptr;
	DXGI_FORMAT curFmt = DXGI_FORMAT_UNKNOWN;
	D3D11_PRIMITIVE_TOPOLOGY curTopo = (D3D11_PRIMITIVE_TOPOLOGY)~0u;
	EntropyAssets::Technique* curTech = nullptr;
	ID3D11ShaderResourceView* s2[] = { this->sky_hemisphere_lookup.Get() };
	if (stage == TfxRenderStage::Transparents || stage == TfxRenderStage::DecalsAdditive) {
		float bf[4] = { 1,1,1,1 };
		ctx->OMSetBlendState(states.blend_states[8].Get(), bf, 0xFFFFFFFF); 
		ctx->OMSetDepthStencilState(depthStencilDecal.Get(), 0);            
		ctx->RSSetState(rasterizerStateGBuffer.Get());                 

		
		ID3D11ShaderResourceView* s0[] = { this->temp_angle_lookup.Get() };
		ID3D11ShaderResourceView* s1[] = { this->gbufA.depth.texCopySRV.Get() };
		
		ctx->PSSetShaderResources(15, 1, s0);
		ctx->PSSetShaderResources(10, 1, s1);
		ctx->PSSetShaderResources(3, 1, s2);
	}

	for (const Item& it : order)
	{
		const DrawPacket& d = *it.p;
		if (d.instanceCount == 0 || d.indexCount == 0) continue;

		if (d.meshId == 0x80EFC095) {
			int bp = 1;
		}
		if (it.key != curKey) {
			if (d.layout != curIL) { ctx->IASetInputLayout(d.layout); curIL = d.layout; }


			if (d.vb0 != curVBs[0] || d.vb1 != curVBs[1] ||
				d.stride0 != curStrides[0] || d.stride1 != curStrides[1]) {
				ID3D11Buffer* vbs[2] = { d.vb0, d.vb1 };
				UINT strides[2] = { d.stride0, d.stride1 };
				UINT offs[2] = { 0,0 };
				const UINT n = d.vb1 ? 2u : 1u;
				ctx->IASetVertexBuffers(0, n, vbs, strides, offs);
				curVBs[0] = d.vb0; curVBs[1] = d.vb1; curStrides[0] = d.stride0; curStrides[1] = d.stride1;
			}

			if (this->staticAO1.ao_buffer && runAmbientOcclusion) {
				ID3D11ShaderResourceView* aoSRV = this->staticAO1.ao_buffer.Get();
				ctx->VSSetShaderResources(1, 1, &aoSRV);
			} else {
				ID3D11ShaderResourceView* nullSRV = nullptr;
				ctx->VSSetShaderResources(1, 1, &nullSRV);
			}
			
			if (d.ib != curIB || d.idxFmt != curFmt) {
				ctx->IASetIndexBuffer(d.ib, d.idxFmt, 0);
				curIB = d.ib; curFmt = d.idxFmt;
			}

			if (d.bVol) ctx->VSSetShaderResources(0, 1, &d.bVol);

			if (d.topo != curTopo) { ctx->IASetPrimitiveTopology(d.topo); curTopo = d.topo; }

			if (d.tech != curTech) 
			{ 
				d.tech->Bind(pDevice, pContext, externs, states, scopes);
				
				curTech = d.tech; 
			}

			curKey = it.key;
		}
		
			
			
		UpdateCB1_StaticReusable(ctx, d.cb1,
			d.worldCount ? &frameWorlds_[d.worldOffset] : nullptr,
			d.worldCount, g_cb1.Get());
		ID3D11Buffer* b1 = g_cb1.Get();
		ctx->VSSetConstantBuffers(1, 1, &b1);
		if (stage == TfxRenderStage::Transparents) {
			ctx->PSSetShaderResources(3, 1, s2);
		}
		ctx->RSSetState(rasterizerStateGBuffer.Get());
		ctx->DrawIndexedInstanced(d.indexCount, d.instanceCount, d.firstIndex, 0, d.baseInstance);
	}

	packets.clear();
}

bool Graphics::ResolveStaticPartOnce(
	const SStaticMeshData& mesh,
	const SStaticMeshPart& part,
	ResolvedStaticPart& out)
{
	if (out.ready) {
		return true;
	}
	const auto& sb = mesh.buffers[part.buffer_index];

	EnsureStaticBufferRegistered(mesh, part, StaticBufKind::Index, D3D11_BIND_INDEX_BUFFER);
	EnsureStaticBufferRegistered(mesh, part, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);
	if (sb.UVBuffer.hash && sb.UVBuffer.hash != 0xFFFFFFFFu)
		EnsureStaticBufferRegistered(mesh, part, StaticBufKind::UV, D3D11_BIND_VERTEX_BUFFER);
	if (sb.VertexColourBuffer.hash && sb.VertexColourBuffer.hash != 0xFFFFFFFFu)
		EnsureStaticBufferRegistered(mesh, part, StaticBufKind::Color, D3D11_BIND_SHADER_RESOURCE);


	auto& fIB = GetOrEnqueueBuffer(sb.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
	auto& fVB0 = GetOrEnqueueBuffer(sb.VertexBuffer.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<ID3D11Buffer>>* fVB1 = nullptr;
	if (sb.UVBuffer.hash && sb.UVBuffer.hash != 0xFFFFFFFFu)
		fVB1 = &GetOrEnqueueBuffer(sb.UVBuffer.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>* fColorSRVPtr = nullptr;
	const bool hasColorId = (sb.VertexColourBuffer.hash != 0u && sb.VertexColourBuffer.hash != 0xFFFFFFFFu);
	if (hasColorId) {
		fColorSRVPtr = &GetOrEnqueueBufferSRV(sb.VertexColourBuffer.hash);
	}
	if (!FutReady(fIB) ||
		!FutReady(fVB0) ||
		(fVB1 && !FutReady(*fVB1)) ||
		(hasColorId && (!fColorSRVPtr || !FutReady(*fColorSRVPtr))))
	{
		return false;
	}

	out.ib = fIB.get();
	out.vb0 = fVB0.get();
	out.vb1 = (fVB1 ? fVB1->get() : nullptr);
	out.vCol = nullptr;
	if (hasColorId && fColorSRVPtr) {
		try {
			out.vCol = fColorSRVPtr->get(); 
		}
		catch (const std::exception& e) {
			printf("Color SRV load failed: %s\n", e.what());
			out.vCol = nullptr;
		}
	}

	const auto pIB = registry->GetBuffer(sb.IndexBuffer.hash);
	const auto pVB0 = registry->GetBuffer(sb.VertexBuffer.hash);
	BufferPayload pVB1{};
	if (out.vb1) pVB1 = registry->GetBuffer(sb.UVBuffer.hash);

	BufferPayload pVC{};
	if (out.vCol) pVC = registry->GetBuffer(sb.VertexColourBuffer.hash);


	out.indexStart = part.index_start;      
	out.indexCount = part.index_count;
	out.stride0 = pVB0.stride;
	out.stride1 = out.vb1 ? pVB1.stride : 0;
	out.idxFmt = (pIB.stride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	
	if (out.vCol) {
		switch (pVC.stride) {
		case 1:  out.vCOlfmt = DXGI_FORMAT_R8_UNORM;        break;
		default: out.vCOlfmt = DXGI_FORMAT_R8G8B8A8_UNORM;               break;
		}
	}

	out.ready = true;
	return true;
}

void Graphics::EnsureStaticBufferRegistered(const SStaticMeshData& mesh,
	const SStaticMeshPart& part,
	StaticBufKind which,
	UINT addFlags)
{
	const auto& sb = mesh.buffers[part.buffer_index];
	TagHash tag =
		(which == StaticBufKind::Index) ? sb.IndexBuffer :
		(which == StaticBufKind::Vertex) ? sb.VertexBuffer :
		(which == StaticBufKind::UV) ? sb.UVBuffer :
		sb.VertexColourBuffer;

	const uint32_t id = tag.hash;
	if (id == 0 || id == 0xFFFFFFFFu) return;

	if (!registry->HasBuffer(id)) {
		auto payload = BuildBufferPayloadFromTag(tag, which);
		payload.desc.BindFlags |= addFlags;     
		registry->RegisterBuffer(id, std::move(payload));
		return;
	}

	auto payload = registry->GetBuffer(id);
	const UINT merged = payload.desc.BindFlags | addFlags;
	if (merged != payload.desc.BindFlags) {
		payload.desc.BindFlags = merged;
		registry->RegisterBuffer(id, std::move(payload));
	}
}

void Graphics::EnsureBufferRegistered(TagHash tag,
	StaticBufKind which,
	UINT addFlags)
{

	const uint32_t id = tag.hash;
	if (id == 0 || id == 0xFFFFFFFFu) return;

	if (!registry->HasBuffer(id)) {
		auto payload = BuildBufferPayloadFromTag(tag, which);
		payload.desc.BindFlags |= addFlags;     
		registry->RegisterBuffer(id, std::move(payload));
		return;
	}

	auto payload = registry->GetBuffer(id);
	const UINT merged = payload.desc.BindFlags | addFlags;
	if (merged != payload.desc.BindFlags) {
		payload.desc.BindFlags = merged;
		registry->RegisterBuffer(id, std::move(payload));
	}
}


void Graphics::EnsureSpecialBufferRegistered(const SStaticSpecial& sp,
	StaticBufKind which,
	UINT addFlags)
{
	TagHash tag{};
	switch (which) {
	case StaticBufKind::Index: tag = sp.IndexBuffer; break;
	case StaticBufKind::Vertex:   tag = sp.VertexBuffer1; break;
	case StaticBufKind::UV:   tag = sp.VertexBuffer2; break;
	case StaticBufKind::Color: tag = sp.VertexColourBuffer; break;
	}

	const uint32_t id = tag.hash;
	if (id == 0u || id == 0xFFFFFFFFu) return;

	if (!registry->HasBuffer(id)) {
		
		StaticBufKind asStatic =
			(which == StaticBufKind::Index) ? StaticBufKind::Index :
			(which == StaticBufKind::Color) ? StaticBufKind::Color :
			StaticBufKind::Vertex; 
		auto payload = BuildBufferPayloadFromTag(tag, asStatic);
		payload.desc.BindFlags |= addFlags;
		registry->RegisterBuffer(id, std::move(payload));
		return;
	}

	auto payload = registry->GetBuffer(id);
	const UINT merged = payload.desc.BindFlags | addFlags;
	if (merged != payload.desc.BindFlags) {
		payload.desc.BindFlags = merged;
		registry->RegisterBuffer(id, std::move(payload));
	}
}

void Graphics::EnsureBufferBind(uint32_t id, UINT addFlags)
{
	if (id == 0 || id == 0xFFFFFFFFu) return;
	BufferPayload payload{};
	if (!registry->TryGetBuffer(id, payload)) return;
	UINT merged = payload.desc.BindFlags | addFlags;
	if (merged != payload.desc.BindFlags) {
		payload.desc.BindFlags = merged;
		registry->RegisterBuffer(id, std::move(payload));
	}
}

std::shared_future<std::shared_ptr<ID3D11Buffer>>&
Graphics::GetOrEnqueueBuffer(uint32_t id, UINT addFlags)
{
	EnsureBufferBind(id, addFlags);
	auto it = bufferFut_.find(id);
	if (it == bufferFut_.end())
		it = bufferFut_.emplace(id, assets->EnqueueBuffer(id).future).first;
	return it->second;
}

void Graphics::PublishGlobalTexturesToFrameExtern()
{
	EnsureFrameCapacity(externs);

	auto setSrv = [&](size_t off, const char* key)
		{
			ID3D11ShaderResourceView* p = nullptr;
			auto it = global_textures.find(key);
			if (it != global_textures.end())
				p = it->second.Get();

			externs.MemcpyScope(TfxExtern::Frame, off, &p, sizeof(p));
		};

	setSrv(FrameOff::kIridescenceLookup, "iridescence_lookup_texture");
	
	setSrv(FrameOff::kSpecularLobeLookup, "specular_lobe_lookup_texture");
	setSrv(FrameOff::kSpecularLobe3DLookup, "specular_lobe_3d_lookup_texture");
	setSrv(FrameOff::kSpecularTintLookup, "specular_tint_lookup_texture");
	ID3D11ShaderResourceView* p = gbufA.depth.texCopySRV.Get();
	externs.MemcpyScope(TfxExtern::Deferred, 0x78, &p, sizeof(p));
}

std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>&
Graphics::GetOrEnqueueBufferSRV(uint32_t id)
{
	BufferPayload payload{};
	if (!registry->TryGetBuffer(id, payload)) return bufferSrvFut_[id]; 
	BufferSRVMeta meta{};
	meta.kind = BufferSRVMeta::Kind::Structured;
	meta.structureByteStride = payload.stride;
	meta.typedFormat = (payload.stride == 1) ? DXGI_FORMAT_R8_UNORM : DXGI_FORMAT_R8G8B8A8_UNORM;
	meta.bytesPerElement = payload.stride;
	EnsureBufferBind(id, D3D11_BIND_SHADER_RESOURCE);
	auto it = bufferSrvFut_.find(id);
	if (it == bufferSrvFut_.end())
		it = bufferSrvFut_.emplace(id, assets->EnqueueBufferSRV(id, meta).future).first;
	return it->second;
}

struct StaticBuffersIDs {
	uint32_t ib = 0xFFFFFFFFu, vb = 0xFFFFFFFFu, uv = 0xFFFFFFFFu, color = 0xFFFFFFFFu;
};
static inline StaticBuffersIDs GetStaticBuffersIDs(const SStaticMeshData& mesh, const SStaticMeshPart& part)
{
	StaticBuffersIDs out{};
	if (part.buffer_index < mesh.buffers.size()) {
		const auto& b = mesh.buffers[part.buffer_index];
		out.ib = b.IndexBuffer.hash;
		out.vb = b.VertexBuffer.hash;
		out.uv = b.UVBuffer.hash;
		out.color = b.VertexColourBuffer.hash;
	}
	return out;
}

static inline void BindGBufferForWriting(ID3D11DeviceContext* ctx, const GBufferRT& gbuf)
{
	ID3D11RenderTargetView* rts[3] = {
		gbuf.rt0.rtv.Get(),
		gbuf.rt1.rtv.Get(),
		gbuf.rt2.rtv.Get()
	};
	ctx->OMSetRenderTargets(3, rts, gbuf.depth.dsv.Get()); 

	D3D11_TEXTURE2D_DESC d{}; gbuf.rt0.tex->GetDesc(&d); 
	D3D11_VIEWPORT vp{ 0,0, float(d.Width), float(d.Height), 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);
}

#pragma once
#define IDB_ANGlE_LOOKUP 102
#define IDB_SKY 105
static inline void SetFullViewport(ID3D11DeviceContext* ctx, float w, float h)
{
	D3D11_VIEWPORT vp{ 0.0f, 0.0f, w, h, 0.0f, 1.0f };
	ctx->RSSetViewports(1, &vp);
}

static std::array<GlobalChannel, 256> channels = GetGlobalChannelDefaults();

static bool TryGetViewExtern(const ExternStorage& ex, ViewExtern& out)
{
	auto it = ex.scopes.find(TfxExtern::View);
	if (it == ex.scopes.end()) return false;
	const auto& v = it->second.cpu;
	if (v.size() < sizeof(ViewExtern)) return false;
	std::memcpy(&out, v.data(), sizeof(ViewExtern));
	return true;
}

Microsoft::WRL::ComPtr<ID3D11Buffer> cb0_override;

auto ShowMat = [](const char* name, const Mat4& m)
	{
		const float* f = reinterpret_cast<const float*>(&m);
		ImGui::Text("%s", name);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[0], f[1], f[2], f[3]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[4], f[5], f[6], f[7]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[8], f[9], f[10], f[11]);
		ImGui::Text("  [ % .4f % .4f % .4f % .4f ]", f[12], f[13], f[14], f[15]);
	};

static inline bool IsReady(const std::shared_future<std::shared_ptr<EntropyAssets::Technique>>& f) {
	using namespace std::chrono_literals;
	return f.valid() && f.wait_for(0s) == std::future_status::ready;
}

std::shared_ptr<EntropyAssets::Technique>
Graphics::GetStaticTechniqueOrEnqueue(uint32_t techId)
{
	if (techId == 0xFFFFFFFFu || techId == 0u) return nullptr;

	auto it = TechCache_.find(techId);
	if (it == TechCache_.end()) {
		TagHash th(techId);
		auto fut = assets->EnqueueTechnique(th);       
		it = TechCache_.emplace(techId, fut).first;
	}

	if (IsReady(it->second)) {
		return it->second.get(); 
	}
	return nullptr;
}

void Graphics::Create1x1SRV(UINT color, ComPtr<ID3D11ShaderResourceView>& address)
{
	D3D11_TEXTURE2D_DESC td = {};
	td.Width = 1; td.Height = 1; td.MipLevels = 1; td.ArraySize = 1;
	td.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	td.SampleDesc = { 1,0 };
	td.Usage = D3D11_USAGE_DEFAULT;
	td.BindFlags = D3D11_BIND_SHADER_RESOURCE;

	D3D11_SUBRESOURCE_DATA init = { &color, 4, 0 };

	Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
	pDevice->CreateTexture2D(&td, &init, tex.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC svd = {};
	svd.Format = td.Format;
	svd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	svd.Texture2D.MipLevels = 1;

	pDevice->CreateShaderResourceView(tex.Get(), &svd, address.GetAddressOf());
}


static void DrawFullscreenTriangle(ID3D11DeviceContext* ctx) {
	ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
	ctx->IASetInputLayout(nullptr);  
	ctx->Draw(4, 0);
}

static constexpr UINT kCB1ByteCapacity = 64 * 1024;

void CreateCB1(ID3D11Device* dev) {
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = kCB1ByteCapacity;            
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	std::array<uint8_t, 64> zero{};            
	D3D11_SUBRESOURCE_DATA init{ nullptr, 0, 0 };
	dev->CreateBuffer(&bd, &init, g_cb1.GetAddressOf());
}

void CreateCB1_FallBack(ID3D11Device* dev) {
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = sizeof(CB1Payload);
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	CB1Payload zero{};
	D3D11_SUBRESOURCE_DATA init{ &zero, 0, 0 };
	dev->CreateBuffer(&bd, &init, g_cb1_fallback.GetAddressOf());
}

void Graphics::InitializeInputLayouts()
{

	for (size_t i = 0; i < INPUT_LAYOUTS.size(); ++i)
	{
		Microsoft::WRL::ComPtr<ID3D11InputLayout> il;

		
		if (INPUT_LAYOUTS[i].elements.empty())
			continue;

		HRESULT hr = CreateInputLayoutFromTigerLayout(pDevice.Get(), INPUT_LAYOUTS[i], il);
		if (FAILED(hr) || !il)
		{
			char msg[256];
			sprintf_s(msg, "Tiger IL[%zu] creation failed (hr=0x%08X)\n", i, (unsigned)hr);
			OutputDebugStringA(msg);
			continue;
		}

		tiger_input_layouts[i] = std::move(il);
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

void Graphics::CreateInstanceBuffer()
{
	const UINT totalBytes = m_instanceStride * m_instanceCapacity;

	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	bd.ByteWidth = totalBytes;
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
	bd.StructureByteStride = m_instanceStride;

	pDevice->CreateBuffer(&bd, nullptr, m_instanceSB.GetAddressOf());

	D3D11_SHADER_RESOURCE_VIEW_DESC sd{};
	sd.ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
	sd.Format = DXGI_FORMAT_UNKNOWN;              
	sd.BufferEx.FirstElement = 0;
	sd.BufferEx.NumElements = m_instanceCapacity;

	pDevice->CreateShaderResourceView(m_instanceSB.Get(), &sd, m_instanceSRV.GetAddressOf());
}

void BuildViewAndProj(View& viewState, const Camera& camera, int windowWidth, int windowHeight)
{
	XMMATRIX V = camera.GetViewMatrix();             
	XMStoreFloat4x4(&viewState.world_to_camera, V);

	const float fovY = XMConvertToRadians(120.0f);
	const float aspect = float(windowWidth) / float(windowHeight);
	const float zn = std::max(0.001f, camera.GetNearZ());

	const float t = tanf(0.5f * fovY);
	const float m11 = 1.0f / (t * aspect);
	const float m22 = 1.0f / t;

	XMMATRIX P = XMMatrixSet(
		m11, 0.0f, 0.0f, 0.0f,
		0.0f, m22, 0.0f, 0.0f,
		0.0f, 0.0f, 0.0f, -1.0f,
		0.0f, 0.0f, zn, 0.0f
	);
	XMStoreFloat4x4(&viewState.camera_to_projective, P);

	
	XMMATRIX VP = XMMatrixMultiply(V, P);           
	XMStoreFloat4x4(&viewState.world_to_projective, VP);
}


void Graphics::DrawStaticMesh(const RenderStatic& rs, const View& view, TfxRenderStage renderStage)
{
	ID3D11DeviceContext* ctx = pContext.Get();
	const auto& mesh = rs.mesh;
	bool farObject = true;
	std::vector<ObjectVectors> visibleWorld;
	{
		using namespace culldbg;
		const XMFLOAT4X4& W2P = view.world_to_projective;
		Frustum fr = Frustum::FromColumnMajor(W2P, true);

		visibleWorld.reserve(rs.world.size());
		for (uint32_t i = 0; i < rs.world.size(); ++i) {
			int failed = -1;
			if (!culldbg::aabb_in_frustum_dbg(fr, rs.bounds[i], &failed, false))
				continue;
			visibleWorld.push_back(rs.world[i]);
			const glm::vec3 objPos = glm::vec3(rs.world[i].translation);
			const float distSq = glm::length2(objPos - frameCameraPos);
			if (distSq <= (lod_distance*lod_distance)) {
				farObject = false;
			}
		}
	}
	if (farObject) {
		return;
	}
	if (visibleWorld.empty()) return;

	
	const UINT count = (UINT)visibleWorld.size();
	if (!m_instWritePtr) return; 

	if (m_instCursor + count > m_instanceCapacity) {
		if (m_instCursor >= m_instanceCapacity) return;
	}

	const UINT  stride = m_instanceStride;
	uint8_t* dst = m_instWritePtr + (size_t)m_instCursor * stride;

	UINT written = 0;
	for (UINT i = 0; i < count && (m_instCursor + i) < m_instanceCapacity; ++i) {
		const ObjectVectors& ov = visibleWorld[i];
		InstanceData* out = reinterpret_cast<InstanceData*>(dst + i * stride);
		out->translation = DirectX::XMFLOAT4(ov.translation.x, ov.translation.y, ov.translation.z, 0.0f);
		out->rotation = DirectX::XMFLOAT4(ov.rotation.x, ov.rotation.y, ov.rotation.z, ov.rotation.w);
		out->scale = ov.scale;
		++written;
	}

	const UINT base = m_instCursor;
	m_instCursor += written;
	if (written == 0) return;

	packets_.reserve(packets_.size() + mesh.mesh_groups.size());

	std::unordered_set<uint32_t> seenPartIdx;
	seenPartIdx.reserve(mesh.mesh_groups.size());

	for (int gi = 0; gi < (int)mesh.mesh_groups.size(); ++gi)
	{
		const auto& mg = mesh.mesh_groups[gi];

		if (mg.TfxRenderStage != (UINT)renderStage) continue;
		
		if (!seenPartIdx.insert(mg.part_index).second) continue; 

		const auto& part = mesh.parts[mg.part_index];

		if (farObject) {
			if (part.LodCatagory < 8) {
				continue;
			}
		}
		else {
			if (part.LodCatagory > 2) {
				continue;
			}
		}

		std::shared_ptr<EntropyAssets::Technique> tech = nullptr;
		if (gi >= 0 && gi < (int)rs.techniques.size())
			tech = GetStaticTechniqueOrEnqueue(rs.techniques[gi]);
		if (!tech) continue;
		const uint64_t cacheKey = (uint64_t(rs.id) << 32) | uint32_t(mg.part_index);
		auto& resolved = staticPartCache_[cacheKey];
		if (!resolved.ready && !ResolveStaticPartOnce(mesh, part, resolved))
			continue;
		const uint32_t worldOffset = (uint32_t)frameWorlds_.size();
		frameWorlds_.insert(frameWorlds_.end(), visibleWorld.begin(), visibleWorld.end());
		const uint32_t worldCount = (uint32_t)visibleWorld.size();
		DrawPacket dp{};
		dp.meshId = rs.id;
		dp.inputLayoutIndex = mg.input_layout_index;
		dp.layout = (mg.input_layout_index < tiger_input_layouts.size() && tiger_input_layouts[mg.input_layout_index])
			? tiger_input_layouts[mg.input_layout_index].Get() : nullptr;
		dp.bVol = resolved.vCol ? resolved.vCol->srv.Get() : nullptr;

		dp.vb0 = resolved.vb0.get();
		dp.vb1 = resolved.vb1 ? resolved.vb1.get() : nullptr;
		dp.stride0 = resolved.stride0;
		dp.stride1 = resolved.stride1;

		dp.cb1.mesh_offset = { rs.mesh.mesh_offset[0], rs.mesh.mesh_offset[1], rs.mesh.mesh_offset[2] };
		dp.cb1.mesh_scale = rs.mesh.mesh_scale;
		dp.cb1.uv_scale = rs.mesh.texture_coordinate_scale;
		dp.cb1.uv_off_x = rs.mesh.texture_coordinate_offset[0];
		dp.cb1.uv_off_y = rs.mesh.texture_coordinate_offset[1];
		dp.cb1.max_colour = mesh.max_colour_index;

		dp.ib = resolved.ib.get();
		dp.idxFmt = resolved.idxFmt;
		dp.worldOffset = worldOffset;
		dp.worldCount = worldCount;   
		dp.baseInstance = 0;
		dp.tech = tech.get();
		dp.topo = part.PrimitiveType == 3 ? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST : D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
		dp.indexCount = resolved.indexCount;
		dp.firstIndex = resolved.indexStart;
		
		dp.instanceCount = (UINT)visibleWorld.size();
		dp.baseInstance = base;
		dp.flags_or_maxColorBits = mesh.max_colour_index;

		

		packets_.push_back(std::move(dp));
	}
	if (!rs.specials.empty())
	{
		const uint32_t spWorldOffset = (uint32_t)frameWorlds_.size();
		frameWorlds_.insert(frameWorlds_.end(), visibleWorld.begin(), visibleWorld.end());
		const uint32_t spWorldCount = (uint32_t)visibleWorld.size();

		for (const auto& specialPtr : rs.specials)
		{
			const StaticSpecial& spWrap = specialPtr;
			const SStaticSpecial& sp = spWrap.part;

			if (sp.TfxRenderStage != (uint8_t)renderStage) continue;

			
			uint32_t techId = (spWrap.techniqueId ? spWrap.techniqueId : sp.technique);
			auto tech = GetStaticTechniqueOrEnqueue(techId);
			if (!tech) continue;

		
			const uint64_t cacheKey = (uint64_t(spWrap.id ? spWrap.id : rs.id) << 32) | uint32_t(sp.index_start);
			auto& resolved = specialsCache_[cacheKey];
			if (!resolved.ready && !ResolveSpecialOnce(sp, resolved))
				continue;

			DrawPacket dp{};
			dp.meshId = spWrap.id ? spWrap.id : rs.id;

			
			const uint16_t ilIdx = spWrap.input_layout_index ? spWrap.input_layout_index
				: sp.input_layout_index;
			dp.inputLayoutIndex = ilIdx;
			dp.layout = (ilIdx < tiger_input_layouts.size() && tiger_input_layouts[ilIdx])
				? tiger_input_layouts[ilIdx].Get()
				: nullptr;
			dp.vb0 = resolved.vb1.get();
			dp.stride0 = resolved.stride1;
			dp.vb1 = resolved.vb2 ? resolved.vb2.get() : nullptr;
			dp.stride1 = resolved.stride2;

			dp.ib = resolved.ib.get();
			dp.idxFmt = resolved.idxFmt;

			
			dp.bVol = (resolved.vCol ? resolved.vCol->srv.Get() : nullptr);

			dp.cb1.mesh_offset = { rs.mesh.mesh_offset[0], rs.mesh.mesh_offset[1], rs.mesh.mesh_offset[2] };
			dp.cb1.mesh_scale = rs.mesh.mesh_scale;
			dp.cb1.uv_scale = rs.mesh.texture_coordinate_scale;
			dp.cb1.uv_off_x = rs.mesh.texture_coordinate_offset[0];
			dp.cb1.uv_off_y = rs.mesh.texture_coordinate_offset[1];
			dp.cb1.max_colour = rs.mesh.max_colour_index;
			dp.worldOffset = spWorldOffset;
			dp.worldCount = spWorldCount;
			dp.instanceCount = (UINT)visibleWorld.size();
			dp.baseInstance = base;

			dp.tech = tech.get();
			dp.topo = (sp.PrimitiveType == 3)
				? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST
				: D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			dp.indexCount = resolved.indexCount;
			dp.firstIndex = resolved.indexStart;

			dp.flags_or_maxColorBits = rs.mesh.max_colour_index;

			dp.sortKeyLow = (renderStage == TfxRenderStage::Transparents)
				? 0u
				: 0u;

			packets_.push_back(std::move(dp));
		}
	}
}

bool Graphics::IsEntityFullyReady(const RenderEntity& rs, TfxRenderStage stage)
{
	for (const SDynamicMesh& dm : rs.meshs)
	{
		size_t start = dm.part_range_per_render_stage[(int)stage];
		size_t end = dm.part_range_per_render_stage[(int)stage + 1];

		for (size_t i = start; i < end; ++i)
		{
			const SDynamicMeshPart& part = dm.parts[i];
			if (part.LodCatagory > 2) continue;

			uint32_t techId =
				(part.varient_shader_index == 0xFFFF)
				? part.technique.hash
				: rs.external_mats[
					rs.external_material_mapping[part.varient_shader_index].technique_start];

			auto itTech = TechCache_.find(techId);
			if (itTech == TechCache_.end())
				return false;              
			if (!IsReady(itTech->second))
				return false;            

			const uint64_t key = (uint64_t(rs.id) << 32) | uint32_t(i);
			auto itDyn = dynamicPartCache_.find(key);
			if (itDyn == dynamicPartCache_.end() || !itDyn->second.ready)
				return false;            
		}
	}
	return true;
}


void Graphics::RunPostprocessChain()
{
	ID3D11DeviceContext* ctx = pContext.Get();
	ScopedGpuEvent G(anno_.Get(), L"postprocess");

	
	{
		ScopedGpuEvent e(anno_.Get(), L"blit_texture_alphaluminance");
		RenderTarget* src; RenderTarget* dst;
		gbufA.GetPostRT(src, dst, true);

		ID3D11RenderTargetView* rtv = dst->rtv.Get();
		ctx->OMSetRenderTargets(1, &rtv, nullptr);
		PP_Viewport(ctx, float(windowWidth), float(windowHeight));
		PP_CommonStates(ctx, bsOpaque.Get(), dsDisabled.Get(), rasterizerStateNoCull.Get());

		if (auto it = globalTechniques.find("blit_texture_alphaluminance");
			it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		ID3D11ShaderResourceView* srvs[] = { gbufA.shading_result.srv.Get() }; 
		ctx->PSSetShaderResources(0, 1, srvs);
		ID3D11SamplerState* samp[] = { samplerLinearClamp.Get() };
		ctx->PSSetSamplers(0, 1, samp);

		PP_SetFS(ctx); PP_DrawFS(ctx);
		UnbindAllSRVs(ctx);
	}

	{
		ScopedGpuEvent e(anno_.Get(), L"debug_overlay_blit_texture");
		RenderTarget* src; RenderTarget* dst;
		gbufA.GetPostRT(src, dst, true);

		ID3D11RenderTargetView* rtv = dst->rtv.Get();
		ctx->OMSetRenderTargets(1, &rtv, nullptr);
		PP_Viewport(ctx, float(windowWidth), float(windowHeight));
		PP_CommonStates(ctx, bsOpaque.Get(), dsDisabled.Get(), rasterizerStateNoCull.Get());

		if (auto it = globalTechniques.find("debug_overlay_blit_texture");
			it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		ID3D11ShaderResourceView* s[] = { src->srv.Get() }; 
		ctx->PSSetShaderResources(0, 1, s);
		ID3D11SamplerState* samp[] = { samplerLinearClamp.Get() };
		ctx->PSSetSamplers(0, 1, samp);

		PP_SetFS(ctx); PP_DrawFS(ctx);
		UnbindAllSRVs(ctx);
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"final_present");
		ID3D11RenderTargetView* bbSRGB = pRenderTargetView.Get();
		ctx->OMSetRenderTargets(1, &bbSRGB, nullptr);
		PP_Viewport(ctx, float(windowWidth), float(windowHeight));
		PP_CommonStates(ctx, bsOpaque.Get(), dsDisabled.Get(), rasterizerStateNoCull.Get());

		auto* out = gbufA.GetPostOutput();
		ID3D11ShaderResourceView* s = out->srv.Get();
		ctx->PSSetShaderResources(0, 1, &s);
		ID3D11SamplerState* samp = samplerLinearClamp.Get();
		ctx->PSSetSamplers(0, 1, &samp);

		PP_SetFS(ctx); PP_DrawFS(ctx);
		UnbindAllSRVs(ctx);
	}
}

void Graphics::CreateTerrainCB64()
{
	D3D11_BUFFER_DESC bd{};
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.ByteWidth = 64;                    
	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
	bd.MiscFlags = 0;
	bd.StructureByteStride = 0;

	
	TerrainCB64 zero{};
	D3D11_SUBRESOURCE_DATA init{};
	init.pSysMem = &zero;

	HRESULT hr = pDevice->CreateBuffer(&bd, &init, g_terrain_cb.GetAddressOf());
	COM_ERROR_IF_FAILED(hr, "CreateTerrainCB64 failed");
}

bool Graphics::ResolveDynamicPartOnce(
	const SDynamicMesh& dm,
	const SDynamicMeshPart& part,
	ResolvedDynamicPart& out)
{

	EnsureEntityBufferRegistered(dm, DynamicBufKind::Index, D3D11_BIND_INDEX_BUFFER);
	EnsureEntityBufferRegistered(dm, DynamicBufKind::Vertex0, D3D11_BIND_VERTEX_BUFFER);
	if (dm.vertex1_buffer.hash && dm.vertex1_buffer.hash != 0xFFFFFFFFu)
		EnsureEntityBufferRegistered(dm, DynamicBufKind::Vertex1, D3D11_BIND_VERTEX_BUFFER);
	if (dm.buffer2.hash && dm.buffer2.hash != 0xFFFFFFFFu)
		EnsureEntityBufferRegistered(dm, DynamicBufKind::Buffer2, D3D11_BIND_VERTEX_BUFFER);
	if (dm.buffer3.hash && dm.buffer3.hash != 0xFFFFFFFFu)
		EnsureEntityBufferRegistered(dm, DynamicBufKind::Buffer3, D3D11_BIND_VERTEX_BUFFER);
	if (dm.colour_buffer.hash && dm.colour_buffer.hash != 0xFFFFFFFFu)
		EnsureEntityBufferRegistered(dm, DynamicBufKind::Color,
			D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE);

	auto& fIB = GetOrEnqueueBuffer(dm.index_buffer.hash, D3D11_BIND_INDEX_BUFFER);
	auto& fVB0 = GetOrEnqueueBuffer(dm.vertex0_buffer.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<ID3D11Buffer>>* fVB1 = nullptr, * fVB2 = nullptr;
	if (dm.vertex1_buffer.hash && dm.vertex1_buffer.hash != 0xFFFFFFFFu)
		fVB1 = &GetOrEnqueueBuffer(dm.vertex1_buffer.hash, D3D11_BIND_VERTEX_BUFFER);
	if (dm.buffer2.hash && dm.buffer2.hash != 0xFFFFFFFFu)
		fVB2 = &GetOrEnqueueBuffer(dm.buffer2.hash, D3D11_BIND_VERTEX_BUFFER);

	std::shared_future<std::shared_ptr<EntropyAssets::BufferSRVRes>>* fCol = nullptr;
	const bool hasCol = (dm.colour_buffer.hash && dm.colour_buffer.hash != 0xFFFFFFFFu);
	if (hasCol) fCol = &GetOrEnqueueBufferSRV(dm.colour_buffer.hash);

	if (!FutReady(fIB) || !FutReady(fVB0) ||
		(fVB1 && !FutReady(*fVB1)) ||
		(fVB2 && !FutReady(*fVB2)) ||
		(hasCol && (!fCol || !FutReady(*fCol))))
		return false;

	out.ib = fIB.get();
	out.vb0 = fVB0.get();
	out.vb1 = (fVB1 ? fVB1->get() : nullptr);
	out.vb2 = (fVB2 ? fVB2->get() : nullptr);
	out.vCol = (hasCol ? fCol->get() : nullptr);

	const auto pIB = registry->GetBuffer(dm.index_buffer.hash);
	const auto pVB0 = registry->GetBuffer(dm.vertex0_buffer.hash);
	out.stride0 = pVB0.stride;
	if (out.vb1) { auto p = registry->GetBuffer(dm.vertex1_buffer.hash); out.stride1 = p.stride; }
	if (out.vb2) { auto p = registry->GetBuffer(dm.buffer2.hash);        out.stride2 = p.stride; }
	out.idxFmt = (pIB.stride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	out.indexStart = part.index_start;
	out.indexCount = part.index_count;
	out.ready = true;
	return true;
}

void Graphics::DrawTerrain(const RenderTerrain& rt, const View& view)
{
	char buf[256];

	if (rt.occlusion_bounds)
	{
		using namespace culldbg;
		const auto fr = Frustum::FromColumnMajor(view.world_to_projective, true);
		int dummy = 0;
		if (!aabb_in_frustum_dbg(fr, *rt.occlusion_bounds, &dummy, false))
			return;
	}


	wchar_t label[128];
	_snwprintf_s(label, _TRUNCATE, L"Draw Terrain %08X [%ls]",
		rt.id, StageName(TfxRenderStage::GenerateGbuffer));
	ScopedGpuEvent _object(anno_.Get(), label);

	EnsureBufferRegistered(rt.meshData.IndexBuffer, StaticBufKind::Index, D3D11_BIND_INDEX_BUFFER);
	EnsureBufferRegistered(rt.meshData.Vertex0, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);
	EnsureBufferRegistered(rt.meshData.Vertex1, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);

	auto& fIB = GetOrEnqueueBuffer(rt.meshData.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
	auto& fV0 = GetOrEnqueueBuffer(rt.meshData.Vertex0.hash, D3D11_BIND_VERTEX_BUFFER);
	auto& fV1 = GetOrEnqueueBuffer(rt.meshData.Vertex1.hash, D3D11_BIND_VERTEX_BUFFER);

	if (!FutReady(fIB) || !FutReady(fV0) || !FutReady(fV1))
		return;

	std::shared_ptr<ID3D11Buffer> ib = fIB.get();
	std::shared_ptr<ID3D11Buffer> vb0 = fV0.get();
	std::shared_ptr<ID3D11Buffer> vb1 = fV1.get();

	const auto pIB = registry->GetBuffer(rt.meshData.IndexBuffer.hash);
	const auto pVB0 = registry->GetBuffer(rt.meshData.Vertex0.hash);
	const auto pVB1 = registry->GetBuffer(rt.meshData.Vertex1.hash);

	const DXGI_FORMAT idxFmt = (pIB.stride == 2) ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;
	const UINT stride0 = pVB0.stride;
	const UINT stride1 = pVB1.stride;

	
	{
		pContext->IASetInputLayout(
			(0xA < tiger_input_layouts.size() && tiger_input_layouts[22])
			? tiger_input_layouts[22].Get()
			: nullptr);

		ID3D11Buffer* vbs[2] = { vb0.get(), vb1.get() };
		UINT strides[2] = { stride0, stride1 };
		UINT offs[2] = { 0,0 };
		pContext->IASetVertexBuffers(0, 2, vbs, strides, offs);

		pContext->IASetIndexBuffer(ib.get(), idxFmt, 0);
	}

	for (const auto& mesh_part : rt.meshData.mesh_parts)
	{
		if (mesh_part.detailLevel >= 1) continue;

		uint32_t techId = mesh_part.Technique;
		auto it = TechCache_.find(techId);
		if (it == TechCache_.end()) {
			TagHash th(techId);
			it = TechCache_.emplace(techId, assets->EnqueueTechnique(th)).first;
		}

		std::shared_ptr<EntropyAssets::Technique> tech;
		try { tech = it->second.get(); }
		catch (...) { continue; }
		if (!tech) continue;
		auto& mesh_group_used = rt.meshData.mesh_groups[mesh_part.groupIndex];
		TerrainCB64 cb = MakeTerrainCB64(
			rt.meshData.transform.x, rt.meshData.transform.y, rt.meshData.transform.z, rt.meshData.transform.w,
			mesh_group_used.Unk20.x, mesh_group_used.Unk20.y, mesh_group_used.Unk20.z, mesh_group_used.Unk20.w
		);

		D3D11_MAPPED_SUBRESOURCE m{};
		if (SUCCEEDED(pContext->Map(g_terrain_cb.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
		{
			std::memcpy(m.pData, &cb, sizeof(cb));
			pContext->Unmap(g_terrain_cb.Get(), 0);
		}
    

		tech->Bind(pDevice, pContext, externs, states, scopes);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

		const UINT firstIndex = (UINT)mesh_part.IndexStart;
		const UINT indexCount = (UINT)mesh_part.IndexCount;
		pContext->RSSetState(rasterizerStateGBuffer.Get());
		if (indexCount == 0) continue;
		const uint32_t gi = mesh_part.groupIndex;
		if (gi >= rt.meshData.mesh_groups.size()) continue;

		ID3D11ShaderResourceView* dyemapSRV = white1x1SRV.Get(); // fallback

		auto& fut = rt.terrain_dyemap_srvs[gi];
		if (FutReady(fut)) {
			std::shared_ptr<EntropyAssets::Texture2DRes> tex;
			try { tex = fut.get(); }
			catch (...) { tex.reset(); }

			if (tex && tex->srv) {
				dyemapSRV = tex->srv.Get();
			}
		}

		ID3D11Buffer* b = g_terrain_cb.Get();
		pContext->VSSetConstantBuffers(11, 1, &b);
		pContext->PSSetShaderResources(14, 1, &dyemapSRV);
		pContext->DrawIndexed(
			indexCount,
			firstIndex,
			0);
	}
}


void Graphics::DrawEntity(const RenderEntity& rs,
	const View& view,
	TfxRenderStage stage,
	uint32_t drawIndex)
{
	bool farObject = true;
	const glm::vec3 objPos = glm::vec3(rs.pos);
	const float distSq = glm::length2(objPos - frameCameraPos);
	if (distSq <= (lod_distance * lod_distance)) {
		farObject = false;
	}
	if (farObject && (rs.rtype != EntityType::SkyEntity)) {
		return; 
	}
	char buf[256];
	if (!IsEntityTypeVisible(rs.rtype))
		return;
	if (rs.occlusion_bounds)
	{
		using namespace culldbg;
		const auto fr = Frustum::FromColumnMajor(view.world_to_projective, true);
		int dummy = 0;
		if (!aabb_in_frustum_dbg(fr, *rs.occlusion_bounds, &dummy, false))
		{
			
			return;
		}
	}

	if (!m_instWritePtr)
	{
		return;
	}

	if (m_instCursor >= m_instanceCapacity)
	{
		return;
	}


	UINT baseInstance = m_instCursor;

	ObjectVectors ov{};
	ov.translation = rs.pos;
	ov.rotation = rs.rot;
	ov.scale = rs.pos.w;

	*reinterpret_cast<InstanceData*>(m_instWritePtr +
		size_t(m_instCursor) * m_instanceStride) =
		InstanceData{
			{ ov.translation.x, ov.translation.y, ov.translation.z, 0.0f },
			{ ov.rotation.x,    ov.rotation.y,    ov.rotation.z,    ov.rotation.w },
			ov.scale
	};

	m_instCursor++;

	uint32_t worldOffset = (uint32_t)frameWorlds_.size();
	frameWorlds_.push_back(ov);

	CB1Payload_override cb1o = BuildCB1FromEntity(rs);

	wchar_t label[128];
	_snwprintf_s(label, _TRUNCATE, L"Draw Dynamic %08X [%ls]", rs.id, StageName(stage));
	ScopedGpuEvent _object(anno_.Get(), label);
	for (size_t meshIndex = 0; meshIndex < rs.meshs.size(); ++meshIndex)
	{
		const SDynamicMesh& dm = rs.meshs[meshIndex];

		size_t start = dm.part_range_per_render_stage[(int)stage];
		size_t end = dm.part_range_per_render_stage[(int)stage + 1];
		const uint8_t ilIdx = dm.input_layout_per_render_stage[(int)stage];
		ID3D11InputLayout* il =
			(ilIdx < tiger_input_layouts.size() && tiger_input_layouts[ilIdx])
			? tiger_input_layouts[ilIdx].Get()
			: nullptr;


		if (!il)
		{
			std::snprintf(buf, sizeof(buf),
				"DrawEntity:   ENT 0x%08X mesh[%zu]: NULL input layout for stage %d (ilIdx=%u)\n",
				rs.id, meshIndex, (int)stage, (unsigned)ilIdx);
			OutputDebugStringA(buf);
		}
		std::shared_ptr<EntropyAssets::Technique> ptech{};
		for (size_t i = start; i < end; ++i)
		{
			const SDynamicMeshPart& part = dm.parts[i];

			if (part.LodCatagory > 3) {
				continue;
			}
			uint32_t	techId;
			if (part.varient_shader_index != 0xFFFF) {
				techId = rs.external_mats[rs.external_material_mapping[part.varient_shader_index].technique_start+rs.varient_index];
				
			}
			else {
				techId = part.technique.hash;
			}
			

			if (rs.rtype == EntityType::ParticleSystem && rs.partical_technique) {
				uint32_t ptechId = *rs.partical_technique;
				
				auto pitTech = TechCache_.find(ptechId);
				if (pitTech == TechCache_.end())
				{
					continue;
				}

				if (!IsReady(pitTech->second))
				{
					continue;
				}

				
				try
				{
					ptech = pitTech->second.get();
				}
				catch (const std::exception& e)
				{

					continue;
				}

				if (!ptech)
				{

					continue;
				}
			}

			auto itTech = TechCache_.find(techId);
			if (itTech == TechCache_.end())
			{
				continue; 
			}

			if (!IsReady(itTech->second))
			{
				continue;
			}

			std::shared_ptr<EntropyAssets::Technique> tech{};
			try
			{
				tech = itTech->second.get();
			}
			catch (const std::exception& e)
			{
				
				continue;
			}

			if (!tech)
			{
				
				continue;
			}

			const uint64_t key = (uint64_t(rs.id) << 32) | uint32_t(i);
			auto itDyn = dynamicPartCache_.find(key);
			if (itDyn == dynamicPartCache_.end() || !itDyn->second.ready)
			{
				

				auto& slot = dynamicPartCache_[key];
				if (!slot.ready)
				{
					if (!ResolveDynamicPartOnce(dm, part, slot))
					{
						
						continue; 
					}
				}
				itDyn = dynamicPartCache_.find(key);
				if (itDyn == dynamicPartCache_.end() || !itDyn->second.ready)
				{
					
					continue;
				}
			}

			ResolvedDynamicPart& resolved = itDyn->second;

			const bool hasSkinning =
				(dm.skinning_buffer.hash != 0u &&
					dm.skinning_buffer.hash != 0xFFFFFFFFu);

			
			UINT offset = 0;
			pContext->IASetInputLayout(il);
			pContext->IASetPrimitiveTopology(
				(part.PrimitiveType == 5)
				? D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP
				: D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			if (resolved.vb1)
			{
				ID3D11Buffer* vbs[2] = { resolved.vb0.get(), resolved.vb1.get() };
				UINT          strides[2] = { resolved.stride0,   resolved.stride1 };
				UINT          offs[2] = { 0,0 };
				pContext->IASetVertexBuffers(0, 2, vbs, strides, offs);
			}
			else
			{
				ID3D11Buffer* vb = resolved.vb0.get();
				UINT          str = resolved.stride0;
				pContext->IASetVertexBuffers(0, 1, &vb, &str, &offset);
			}

			
			tech->Bind_With_Channels(pDevice, pContext, externs, states, scopes, rs.channels);
			
			if (hasSkinning && entity_vs_override)
			{
				pContext->VSSetShader(entity_vs_override.Get(), nullptr, 0);

				D3D11_MAPPED_SUBRESOURCE m{};
				if (SUCCEEDED(pContext->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
				{
					memcpy(m.pData, &cb1o, sizeof(cb1o));
					pContext->Unmap(g_cb1.Get(), 0);
				}
				ID3D11Buffer* c1 = g_cb1.Get();
				pContext->VSSetConstantBuffers(1, 1, &c1);
			}
			else
			{
				D3D11_MAPPED_SUBRESOURCE m{};
				if (SUCCEEDED(pContext->Map(g_cb1.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &m)))
				{
					memcpy(m.pData, &rs.cb1_single, sizeof(rs.cb1_single));
					pContext->Unmap(g_cb1.Get(), 0);
				}
				ID3D11Buffer* c1 = g_cb1.Get();
				pContext->VSSetConstantBuffers(1, 1, &c1);
			}

			pContext->IASetIndexBuffer(resolved.ib.get(), resolved.idxFmt, 0);

			if (resolved.vCol)
				pContext->VSSetShaderResources(0, 1, resolved.vCol->srv.GetAddressOf());

			pContext->DrawIndexedInstanced(
				resolved.indexCount,
				1,
				resolved.indexStart,
				0,
				baseInstance);
			if (selectedEntitySettingsOpen &&
				selectedEntityIndex >= 0 &&
				(uint32_t)selectedEntityIndex == drawIndex &&
				rasterizerStateWireframe &&
				depthStencilReadOnly &&
				bsAdditive)
			
			{
				Microsoft::WRL::ComPtr<ID3D11RasterizerState> prevRS;
				Microsoft::WRL::ComPtr<ID3D11DepthStencilState> prevDS;
				Microsoft::WRL::ComPtr<ID3D11BlendState> prevBS;
				UINT prevStencil = 0;
				float prevBlend[4] = { 0,0,0,0 };
				UINT prevSampleMask = 0;

				pContext->RSGetState(prevRS.GetAddressOf());
				pContext->OMGetDepthStencilState(prevDS.GetAddressOf(), &prevStencil);
				pContext->OMGetBlendState(prevBS.GetAddressOf(), prevBlend, &prevSampleMask);

				float one[4] = { 1,1,1,1 };
				pContext->RSSetState(rasterizerStateWireframe.Get());
				pContext->OMSetDepthStencilState(depthStencilReadOnly.Get(), 0);
				pContext->OMSetBlendState(bsAdditive.Get(), one, 0xFFFFFFFF);

				pContext->DrawIndexedInstanced(
					resolved.indexCount,
					1,
					resolved.indexStart,
					0,
					baseInstance);

				pContext->RSSetState(prevRS.Get());
				pContext->OMSetDepthStencilState(prevDS.Get(), prevStencil);
				pContext->OMSetBlendState(prevBS.Get(), prevBlend, prevSampleMask);
			}



		}
	}
}


void Graphics::PrewarmVisibleAssets(const View& view)
{
	using namespace culldbg;

	const XMFLOAT4X4& W2P = view.world_to_projective;
	Frustum fr = Frustum::FromColumnMajor(W2P, true);

	
	
	
	for (auto& rs : staticsToDraw)
	{
		if (g_activations_this_frame >= g_activation_budget_per_frame)
			break;

		bool anyVisible = false;
		for (uint32_t i = 0; i < rs.world.size(); ++i) {
			if (aabb_in_frustum_dbg(fr, rs.bounds[i], nullptr, false)) {
				anyVisible = true;
				break;
			}
		}
		if (!anyVisible) continue;

		for (int gi = 0; gi < (int)rs.mesh.mesh_groups.size(); ++gi)
		{
			if (g_activations_this_frame >= g_activation_budget_per_frame)
				break;

			const auto& mg = rs.mesh.mesh_groups[gi];
			if (mg.TfxRenderStage != (UINT)TfxRenderStage::GenerateGbuffer) continue;

			std::shared_ptr<EntropyAssets::Technique> tech{};
			if (gi >= 0 && gi < (int)rs.techniques.size()) {
				auto it = TechCache_.find(rs.techniques[gi]);
				if (it == TechCache_.end()) {
					TagHash th(rs.techniques[gi]);
					it = TechCache_.emplace(rs.techniques[gi], assets->EnqueueTechnique(th)).first;
				}
				if (IsReady(it->second) && g_activations_this_frame < g_activation_budget_per_frame) {
					tech = it->second.get();
					++g_activations_this_frame;
				}
			}

			const auto& part = rs.mesh.parts[mg.part_index];
			const auto& sb = rs.mesh.buffers[part.buffer_index];

			EnsureStaticBufferRegistered(rs.mesh, part, StaticBufKind::Index, D3D11_BIND_INDEX_BUFFER);
			EnsureStaticBufferRegistered(rs.mesh, part, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);
			if (sb.UVBuffer.hash && sb.UVBuffer.hash != 0xFFFFFFFFu)
				EnsureStaticBufferRegistered(rs.mesh, part, StaticBufKind::UV, D3D11_BIND_VERTEX_BUFFER);
			if (sb.VertexColourBuffer.hash && sb.VertexColourBuffer.hash != 0xFFFFFFFFu)
				EnsureStaticBufferRegistered(rs.mesh, part, StaticBufKind::Color, D3D11_BIND_SHADER_RESOURCE);

			auto& fIB = GetOrEnqueueBuffer(sb.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
			auto& fVB = GetOrEnqueueBuffer(sb.VertexBuffer.hash, D3D11_BIND_VERTEX_BUFFER);
			std::shared_ptr<ID3D11Buffer> tmp;
			TryActivateReady(fIB, tmp);
			TryActivateReady(fVB, tmp);

			if (sb.UVBuffer.hash && sb.UVBuffer.hash != 0xFFFFFFFFu) {
				auto& fUV = GetOrEnqueueBuffer(sb.UVBuffer.hash, D3D11_BIND_VERTEX_BUFFER);
				TryActivateReady(fUV, tmp);
			}
			if (sb.VertexColourBuffer.hash && sb.VertexColourBuffer.hash != 0xFFFFFFFFu) {
				auto& fCol = GetOrEnqueueBufferSRV(sb.VertexColourBuffer.hash);
				std::shared_ptr<EntropyAssets::BufferSRVRes> tmpSRV;
				TryActivateReady(fCol, tmpSRV);
			}
		}
	}

	
	
	
	for (auto& rs : staticsToDraw)
	{
		if (g_activations_this_frame >= g_activation_budget_per_frame)
			break;

		if (rs.specials.empty()) continue;

		bool anyVisible = false;
		for (uint32_t i = 0; i < rs.world.size(); ++i) {
			if (aabb_in_frustum_dbg(fr, rs.bounds[i], nullptr, false)) {
				anyVisible = true;
				break;
			}
		}
		if (!anyVisible) continue;

		for (const auto& specialPtr : rs.specials)
		{
			if (g_activations_this_frame >= g_activation_budget_per_frame)
				break;

			const SStaticSpecial& sp = specialPtr.part;
			if (sp.TfxRenderStage != (uint8_t)TfxRenderStage::GenerateGbuffer) continue;

			uint32_t techId = (specialPtr.techniqueId ? specialPtr.techniqueId : sp.technique);
			auto it = TechCache_.find(techId);
			if (it == TechCache_.end()) {
				TagHash th(techId);
				it = TechCache_.emplace(techId, assets->EnqueueTechnique(th)).first;
			}
			if (IsReady(it->second) && g_activations_this_frame < g_activation_budget_per_frame) {
				(void)it->second.get();
				++g_activations_this_frame;
			}

			EnsureSpecialBufferRegistered(sp, StaticBufKind::Index, D3D11_BIND_INDEX_BUFFER);
			EnsureSpecialBufferRegistered(sp, StaticBufKind::Vertex, D3D11_BIND_VERTEX_BUFFER);
			if (sp.VertexBuffer2.hash && sp.VertexBuffer2.hash != 0xFFFFFFFFu)
				EnsureSpecialBufferRegistered(sp, StaticBufKind::UV, D3D11_BIND_VERTEX_BUFFER);
			if (sp.VertexColourBuffer.hash && sp.VertexColourBuffer.hash != 0xFFFFFFFFu)
				EnsureSpecialBufferRegistered(sp, StaticBufKind::Color, D3D11_BIND_SHADER_RESOURCE);

			auto& fIB = GetOrEnqueueBuffer(sp.IndexBuffer.hash, D3D11_BIND_INDEX_BUFFER);
			auto& fVB1 = GetOrEnqueueBuffer(sp.VertexBuffer1.hash, D3D11_BIND_VERTEX_BUFFER);
			std::shared_ptr<ID3D11Buffer> tmpBuf;
			TryActivateReady(fIB, tmpBuf);
			TryActivateReady(fVB1, tmpBuf);

			if (sp.VertexBuffer2.hash && sp.VertexBuffer2.hash != 0xFFFFFFFFu) {
				auto& fVB2 = GetOrEnqueueBuffer(sp.VertexBuffer2.hash, D3D11_BIND_VERTEX_BUFFER);
				TryActivateReady(fVB2, tmpBuf);
			}
			if (sp.VertexColourBuffer.hash && sp.VertexColourBuffer.hash != 0xFFFFFFFFu) {
				auto& fCol = GetOrEnqueueBufferSRV(sp.VertexColourBuffer.hash);
				std::shared_ptr<EntropyAssets::BufferSRVRes> tmpSRV;
				TryActivateReady(fCol, tmpSRV);
			}
		}
	}

	
	
	
	static std::unordered_set<uint64_t> s_dynBufRegistered;

for (auto& ent : entitiesToDraw)
{
    bool visible = true;
    if (ent.occlusion_bounds) {
        visible = aabb_in_frustum_dbg(fr, *ent.occlusion_bounds, nullptr, false);
    }
    if (!visible)
        continue;

   
    for (size_t m = 0; m < ent.meshs.size(); ++m)
    {
        const SDynamicMesh& dm = ent.meshs[m];

       
        const uint64_t regKey = (uint64_t(ent.id) << 32) | uint32_t(m);
        if (!s_dynBufRegistered.count(regKey)) {
            EnsureEntityBufferRegistered(dm, DynamicBufKind::Index,   D3D11_BIND_INDEX_BUFFER);
            EnsureEntityBufferRegistered(dm, DynamicBufKind::Vertex0, D3D11_BIND_VERTEX_BUFFER);
            if (dm.vertex1_buffer.hash && dm.vertex1_buffer.hash != 0xFFFFFFFFu)
                EnsureEntityBufferRegistered(dm, DynamicBufKind::Vertex1, D3D11_BIND_VERTEX_BUFFER);
            if (dm.buffer2.hash && dm.buffer2.hash != 0xFFFFFFFFu)
                EnsureEntityBufferRegistered(dm, DynamicBufKind::Buffer2, D3D11_BIND_VERTEX_BUFFER);
            if (dm.buffer3.hash && dm.buffer3.hash != 0xFFFFFFFFu)
                EnsureEntityBufferRegistered(dm, DynamicBufKind::Buffer3, D3D11_BIND_VERTEX_BUFFER);
            if (dm.colour_buffer.hash && dm.colour_buffer.hash != 0xFFFFFFFFu)
                EnsureEntityBufferRegistered(dm, DynamicBufKind::Color,
                    D3D11_BIND_VERTEX_BUFFER | D3D11_BIND_SHADER_RESOURCE);
            
            

            s_dynBufRegistered.insert(regKey);
        }

        
        size_t starts[2] = {
            dm.part_range_per_render_stage[(int)TfxRenderStage::GenerateGbuffer],
            dm.part_range_per_render_stage[(int)TfxRenderStage::Transparents]
        };
        size_t ends[2] = {
            dm.part_range_per_render_stage[(int)TfxRenderStage::GenerateGbuffer + 1],
            dm.part_range_per_render_stage[(int)TfxRenderStage::Transparents + 1]
        };


        for (int s = 0; s < 2; ++s)
        {
            for (size_t i = starts[s]; i < ends[s]; ++i)
            {
                const auto& part = dm.parts[i];
                if (part.LodCatagory > 3)
                    continue;
				const uint64_t key = ((uint64_t)ent.id << 32) | uint32_t(i);
				
                uint32_t techId;
                if (part.varient_shader_index == 0xFFFF) {
                    techId = part.technique.hash;
                }
                else {
                    techId = ent.external_mats[
                        ent.external_material_mapping[part.varient_shader_index].technique_start+ent.varient_index];
                }

                auto it = TechCache_.find(techId);
                if (it == TechCache_.end()) {
                    TagHash th(techId);
                    it = TechCache_.emplace(techId, assets->EnqueueTechnique(th)).first;
                }
				auto& resolved = dynamicPartCache_[key];
				if (resolved.ready) {
					continue;
				}
               
                if (IsReady(it->second) && g_activations_this_frame < g_activation_budget_per_frame)
                {
                    try {
                        (void)it->second.get();
                        ++g_activations_this_frame;
                    }
                    catch (const std::exception& e) {
                        char buf[256];
                        std::snprintf(buf, sizeof(buf),
                            "PrewarmVisibleAssets ENT 0x%08X: tech 0x%08X activation failed: %s\n",
                            ent.id, techId, e.what());
                        
                    }
                }

                
                auto tryVB = [&](TagHash h, UINT flags)
                {
                    if (!h.hash || h.hash == 0xFFFFFFFFu) return;
                    auto& f = GetOrEnqueueBuffer(h.hash, flags);
                    std::shared_ptr<ID3D11Buffer> tmp;
                    TryActivateReady(f, tmp);
                };

                tryVB(dm.vertex0_buffer, D3D11_BIND_VERTEX_BUFFER);
                tryVB(dm.vertex1_buffer, D3D11_BIND_VERTEX_BUFFER);
                
                tryVB(dm.buffer3,        D3D11_BIND_VERTEX_BUFFER);

                auto trySRV = [&](TagHash h)
                {
                    if (!h.hash || h.hash == 0xFFFFFFFFu) return;
                    auto& f = GetOrEnqueueBufferSRV(h.hash);
                    std::shared_ptr<EntropyAssets::BufferSRVRes> tmp;
                    TryActivateReady(f, tmp);
                };

                trySRV(dm.colour_buffer);
               
                
                resolved = dynamicPartCache_[key];
				if (!resolved.ready)
					printf("Resolved not ready");
                    ResolveDynamicPartOnce(dm, part, resolved);
            }
        }
    }
	if (ent.partical_technique) {
		auto it = TechCache_.find(*ent.partical_technique);
		if (it == TechCache_.end()) {
			TagHash th(*ent.partical_technique);
			it = TechCache_.emplace(*ent.partical_technique, assets->EnqueueTechnique(th)).first;
		}
	}
	}
	for (auto& rt : terrainToDraw) // must be &
	{
		rt.terrain_dyemap_srvs.clear();
		rt.terrain_dyemap_srvs.reserve(rt.meshData.mesh_groups.size());

		for (auto& mg : rt.meshData.mesh_groups)
		{
			const uint32_t id = mg.dyemap.hash;

			if (id == 0u || id == 0xFFFFFFFFu) {
				rt.terrain_dyemap_srvs.emplace_back(); 
				continue;
			}

			if (!registry->HasTexture(id)) {
				auto payload = BuildTexturePayloadFromTag(mg.dyemap);
				registry->RegisterTexture(id, *payload);
			}

			auto req = assets->EnqueueTexture(id);
			rt.terrain_dyemap_srvs.push_back(req.future);
		}
	}

}	

void Graphics::EnsureEntityBufferRegistered(const SDynamicMesh& mesh,
	DynamicBufKind which,
	UINT addFlags)
{
	TagHash tag = GetEntityBufTag(mesh, which);
	const uint32_t id = tag.hash;
	if (id == 0u || id == 0xFFFFFFFFu) return;

	if (!registry->HasBuffer(id)) {
	
		StaticBufKind parseAs =
			(which == DynamicBufKind::Index) ? StaticBufKind::Index :
			(which == DynamicBufKind::Color) ? StaticBufKind::Color :
			StaticBufKind::Vertex;

		auto payload = BuildBufferPayloadFromTag(tag, parseAs);

		payload.desc.BindFlags |= addFlags;

		registry->RegisterBuffer(id, std::move(payload));
		return;
	}

	auto payload = registry->GetBuffer(id);
	const UINT merged = payload.desc.BindFlags | addFlags;
	if (merged != payload.desc.BindFlags) {
		payload.desc.BindFlags = merged;
		registry->RegisterBuffer(id, std::move(payload));
	}
}



void Graphics::RenderFrame()
{
	static bool drawrt1 = false, drawrt0 = false, drawrt2 = false, drawLight_diffuse = false,
		drawLight_specular = false, drawDepth = false, drawShading = false,
		drawShadingRead = false, drawLight_ibl = false, stageGlobalLighting = true, drawEntityLabels = false;
	
	
	mainQueue->RunSlice(64, 4);
	auto pos = camera.GetPositionFloat3();
	frameCameraPos.x = pos.x;
	frameCameraPos.y = pos.y;
	frameCameraPos.z = pos.z;
	gTimer.tick();
	g_activations_this_frame = 0;
	
	tfx::ResetGlobalChannelUsagePerFrame();
	packets_.clear();
	frameWorlds_.clear();
	
	externs.SetFrameTimes(float(gTimer.total_game_time()), float(gTimer.delta_game_time()), 4.0f);
	externs.SetFxaa(float(gTimer.total_game_time()));

	View viewState{};

	XMStoreFloat4x4(&viewState.world_to_camera, camera.GetViewMatrix());
	const float fovY = DirectX::XMConvertToRadians(90.0f);
	const float aspect = float(windowWidth) / float(windowHeight);
	const float zn = std::max(0.001f, camera.GetNearZ());

	const float t = tanf(fovY * 0.5f);
	const float m11 = 1.0f / (t * aspect); 
	const float m22 = 1.0f / t;            

	DirectX::XMMATRIX proj = DirectX::XMMatrixSet(
		m11, 0.0f, 0.0f, 0.0f,   
		0.0f, m22, 0.0f, 0.0f,   
		0.0f, 0.0f, 0.0f, -1.0f, 
		0.0f, 0.0f, zn, 0.0f   
	);

	XMStoreFloat4x4(&viewState.camera_to_projective, proj);
	viewState.derive_matrices_vs({ { float(windowWidth), float(windowHeight) } });

	{
		
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;

		externs.SetViewProjectiveToCamera(viewState, vp);

		
		externs.EnsureAll(pDevice.Get());
		externs.UploadAll(pContext.Get());
	}
	bool sceneEmpty = staticsToDraw.empty() && entitiesToDraw.empty() && lightsToDraw.empty();
	
	if (!sceneEmpty) {
		for (auto& scope : scopes)
			scope.second.UpdateScopeBuffers(pContext, externs);
	}

	PrewarmVisibleAssets(viewState);

	PublishGlobalTexturesToFrameExtern();
	
	ID3D11ShaderResourceView* vsSrvs[] = { m_instanceSRV.Get() };
	pContext->VSSetShaderResources(15, 1, vsSrvs); 

	
	
	
	

	if (!sceneEmpty) {
		ScopedGpuEvent e(anno_.Get(), L"generate_gbuffer");

		m_instCursor = 0;
		D3D11_MAPPED_SUBRESOURCE map{};
		pContext->Map(m_instanceSB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		m_instWritePtr = static_cast<uint8_t*>(map.pData);
		ID3D11ShaderResourceView* srvs[] = { m_instanceSRV.Get() };
		pContext->VSSetShaderResources(15, 1, srvs); 

		ID3D11RenderTargetView* rt_gbuf[3]{
			gbufA.rt0.rtv.Get(),
			gbufA.rt1.rtv.Get(),
			gbufA.rt2.rtv.Get()
		};
		pContext->OMSetRenderTargets(3, rt_gbuf, gbufA.depth.dsv.Get());

		D3D11_VIEWPORT vp{ 0,0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);

		const float clearRt0[4] = { 0, 0, 0, 1 };
		pContext->ClearRenderTargetView(gbufA.rt0.rtv.Get(), clearRt0);

		
		const float clearRt1[4] = { 0.5f, 0.5f, 1.0f, 1.0f };
		pContext->ClearRenderTargetView(gbufA.rt1.rtv.Get(), clearRt1);

		
		const float clearRt2[4] = { 0.0f, 0.0f, 1.0f, 0.0f };
		pContext->ClearRenderTargetView(gbufA.rt2.rtv.Get(), clearRt2);

		
		pContext->ClearDepthStencilView(
			gbufA.depth.dsv.Get(),
			D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL,
			0.0f, 0);

		
		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(states.blend_states[0].Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(depthStencilState.Get(), 0);
		pContext->RSSetState(rasterizerStateGBuffer.Get());

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		
		
		if (selectedEntityIndex >= 0) {
			if (selectedEntityIndex >= (int32_t)entitiesToDraw.size() ||
				entitiesToDraw[(size_t)selectedEntityIndex].id != selectedEntityId ||
				entitiesToDraw[(size_t)selectedEntityIndex].rtype != selectedEntityType)
			{
				float bestD2 = FLT_MAX;
				int32_t bestIdx = -1;
				for (size_t j = 0; j < entitiesToDraw.size(); ++j) {
					const auto& e = entitiesToDraw[j];
					if (e.id != selectedEntityId || e.rtype != selectedEntityType) continue;
					const float dx = e.pos.x - selectedEntityPos.x;
					const float dy = e.pos.y - selectedEntityPos.y;
					const float dz = e.pos.z - selectedEntityPos.z;
					const float d2 = dx*dx + dy*dy + dz*dz;
					if (d2 < bestD2) { bestD2 = d2; bestIdx = (int32_t)j; }
				}
				selectedEntityIndex = bestIdx;
			}
		}

		for (auto& rs : staticsToDraw) DrawStaticMesh(rs, viewState, TfxRenderStage::GenerateGbuffer);
		for (auto& rt : this->terrainToDraw) {
			DrawTerrain(rt, viewState);
		}
	}
		
		SubmitPackets(pContext.Get(), packets_,TfxRenderStage::GenerateGbuffer);
		for (size_t entIdx = 0; entIdx < entitiesToDraw.size(); ++entIdx) {
			auto& re = entitiesToDraw[entIdx];
			DrawEntity(re, viewState, TfxRenderStage::GenerateGbuffer, (uint32_t)entIdx);
		}
		pContext->Unmap(m_instanceSB.Get(), 0);
		m_instWritePtr = nullptr;

		
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (RT1->RT1_Clone)");
		pContext->CopyResource(gbufA.rt1_read.tex.Get(), gbufA.rt1.tex.Get());
	}
	{
		ScopedGpuEvent e(anno_.Get(), L"copy (Depth->DepthCopySRV)");
		if (gbufA.depth.texCopy && gbufA.depth.tex)
			pContext->CopyResource(gbufA.depth.texCopy.Get(), gbufA.depth.tex.Get());
	}
	{   
		for (auto& rs : staticsToDraw) DrawStaticMesh(rs, viewState, TfxRenderStage::Decals);
	}
	
	
	
	
	if (!sceneEmpty) {
		ScopedGpuEvent e(anno_.Get(), L"lighting_pass");

		ID3D11RenderTargetView* rts[2] = {
			gbufA.light_diffuse.rtv.Get(),
			gbufA.light_specular.rtv.Get(),
		};
		pContext->OMSetRenderTargets(2, rts, nullptr);

		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));

		const float black[4] = { 0,0,0,0 };
		const float dim[4] = { 0.001f,0.001f,0.001f,0.0f };
		pContext->ClearRenderTargetView(gbufA.light_diffuse.rtv.Get(), dim);
		pContext->ClearRenderTargetView(gbufA.light_specular.rtv.Get(), black);
		pContext->ClearRenderTargetView(gbufA.light_ibl_specular.rtv.Get(), black);
		if (stageGlobalLighting) {
			if (auto it = globalTechniques.find("global_lighting"); it != globalTechniques.end())
				it->second.get()->Bind(pDevice, pContext, externs, states, scopes);
		}

		pContext->OMSetDepthStencilState(depthStencilStateLighting.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());


		ID3D11ShaderResourceView* srvs[] = {
			gbufA.rt2.srv.Get(),          
			gbufA.rt1_read.srv.Get(),     
			gbufA.depth.texCopySRV.Get(), 
			white1x1SRV.Get(),            
			white1x1SRV.Get(),            
			white1x1SRV.Get(),            
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs), srvs);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);

		ID3D11SamplerState* sams2[] = { lighting1.Get(), lighting2.Get() };
		pContext->PSSetSamplers(0, (UINT)std::size(sams2), sams2);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		for (auto& light : lightsToDraw)
			DrawLight(light, viewState,TfxRenderStage::LightingApply);
	}

	
	if (!sceneEmpty) {
		ScopedGpuEvent e(anno_.Get(), L"deferred_shading");

		ID3D11RenderTargetView* rt = gbufA.shading_result.rtv.Get();
		pContext->OMSetRenderTargets(1, &rt, nullptr);

		if (auto it = globalTechniques.find("deferred_shading_no_atm"); it != globalTechniques.end())
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);

		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		D3D11_VIEWPORT vp{ 0,0, float(windowWidth), float(windowHeight), 0.0f, 1.0f };
		pContext->RSSetViewports(1, &vp);

		ID3D11ShaderResourceView* srvs2[] = {
			gbufA.rt1_read.srv.Get(),          
			gbufA.rt0.srv.Get(),               
			gbufA.rt2.srv.Get(),               
			gbufA.light_diffuse.srv.Get(),     
			gbufA.light_specular.srv.Get(),    
			gbufA.light_ibl_specular.srv.Get(),
			this->global_textures.find("iridescence_lookup_texture")->second.Get(), 
			white1x1SRV.Get(),                 
		};
		pContext->PSSetShaderResources(0, (UINT)std::size(srvs2), srvs2);

		ID3D11SamplerState* samsShade[] = { shading1.Get(), shading2.Get() };
		pContext->PSSetSamplers(0, (UINT)std::size(samsShade), samsShade);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nulls[8] = {};
		pContext->PSSetShaderResources(0, 8, nulls);

		pContext->CopyResource(gbufA.shading_result_read.tex.Get(), gbufA.shading_result.tex.Get());

		externs.SetTransparentSRVs(
			externs,
			gbufA.atmos_ss_far_lookup.srv.Get(),             
			nullptr, 
			gbufA.atmos_ss_far_lookup.srv.Get(),            
			nullptr,
			nullptr,
			this->grey1x1SRV.Get(),
			this->grey1x1SRV.Get(),                 
			this->grey1x1SRV.Get(),                
			nullptr,            
			nullptr,
			nullptr ,
			this->grey1x1SRV.Get(),                  
			gbufA.shading_result_read.srv.Get()             
		);
	}

	
	
	
	if (!sceneEmpty) {

		ID3D11ShaderResourceView* nulls[16] = {};
		pContext->PSSetShaderResources(0, 16, nulls);
		pContext->VSSetShaderResources(0, 16, nulls);

		ID3D11RenderTargetView* rt[1] = { gbufA.shading_result.rtv.Get() };
		pContext->OMSetRenderTargets(1, rt, gbufA.depth.dsv.Get());

		ScopedGpuEvent e(anno_.Get(), L"transparency_pass");
		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));


		m_instCursor = 0;
		D3D11_MAPPED_SUBRESOURCE map{};
		pContext->Map(m_instanceSB.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &map);
		m_instWritePtr = static_cast<uint8_t*>(map.pData);


		ID3D11ShaderResourceView* srvs[] = { m_instanceSRV.Get() };
		pContext->VSSetShaderResources(15, 1, srvs);
		ID3D11ShaderResourceView* srv_stage[] = { gbufA.rt0.srv.Get() };
		pContext->PSSetShaderResources(23, 1, srv_stage); 
		{
			ScopedGpuEvent e(anno_.Get(), L"decals_additive");
			pContext->RSSetState(rasterizerStateGBuffer.Get());
			{
				for (auto& rs : staticsToDraw) DrawStaticMesh(rs, viewState, TfxRenderStage::DecalsAdditive);
			}
			for (size_t entIdx = 0; entIdx < entitiesToDraw.size(); ++entIdx) {
				auto& re = entitiesToDraw[entIdx];
				pContext->RSSetState(rasterizerStateGBuffer.Get());
				DrawEntity(re, viewState, TfxRenderStage::DecalsAdditive, (uint32_t)entIdx);
			}
			SubmitPackets(pContext.Get(), packets_, TfxRenderStage::DecalsAdditive);
		}
		
		for (auto& rs : staticsToDraw) {
			pContext->RSSetState(rasterizerStateGBuffer.Get());
			DrawStaticMesh(rs, viewState, TfxRenderStage::Transparents);
		}
		
		SubmitPackets(pContext.Get(), packets_, TfxRenderStage::Transparents);
		for (size_t entIdx = 0; entIdx < entitiesToDraw.size(); ++entIdx) {
			auto& re = entitiesToDraw[entIdx];
			pContext->RSSetState(rasterizerStateGBuffer.Get());
			DrawEntity(re, viewState, TfxRenderStage::Transparents, (uint32_t)entIdx);
		}

		pContext->Unmap(m_instanceSB.Get(), 0);
		m_instWritePtr = nullptr;

		//for (auto& light : lightsToDraw) {
			//DrawLight(light, viewState, TfxRenderStage::Volumetrics);
		//}

		pContext->PSSetShaderResources(0, 16, nulls);
		pContext->VSSetShaderResources(0, 16, nulls);
	}

	if (!sceneEmpty) {
		ScopedGpuEvent e(anno_.Get(), L"postprocess");
		RunPostprocessChain();
	}
	
	{
		ScopedGpuEvent e(anno_.Get(), L"present_to_backbuffer");

		
		ID3D11RenderTargetView* bbSRGB = pRenderTargetView.Get();
		pContext->OMSetRenderTargets(1, &bbSRGB, nullptr);

		D3D11_VIEWPORT vp{};
		vp.TopLeftX = 0.0f; vp.TopLeftY = 0.0f;
		vp.Width = float(windowWidth);
		vp.Height = float(windowHeight);
		vp.MinDepth = 0.0f; vp.MaxDepth = 1.0f;
		pContext->RSSetViewports(1, &vp);

		const float clear[4] = { 0,0,0,1 };
		pContext->ClearRenderTargetView(bbSRGB, clear);

		if (auto it = globalTechniques.find("final_combine"); it != globalTechniques.end()) {
			it->second.get()->Bind(pDevice, pContext, externs, states, scopes);
		}
		float bf[4] = { 1,1,1,1 };
		pContext->OMSetBlendState(bsOpaque.Get(), bf, 0xFFFFFFFF);
		pContext->OMSetDepthStencilState(dsDisabled.Get(), 0);
		pContext->RSSetState(rasterizerStateNoCull.Get());

		ID3D11ShaderResourceView* src = gbufA.shading_result.srv.Get();
		pContext->PSSetShaderResources(0, 1, &src);
		ID3D11SamplerState* samp = samplerLinearClamp.Get();
		pContext->PSSetSamplers(0, 1, &samp);

		pContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		pContext->IASetInputLayout(nullptr);
		pContext->Draw(4, 0);

		ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
		pContext->PSSetShaderResources(0, 1, nullSRV);
	}

	const bool wantDebugPreview =
		drawrt0 || drawrt1 || drawrt2 || drawLight_diffuse || drawLight_specular ||
		drawLight_ibl || drawShading || drawShadingRead;


	if (wantDebugPreview)
	{
		ID3D11RenderTargetView* bbLinear = pRenderTargetViewLinear.Get(); 
		pContext->OMSetRenderTargets(1, &bbLinear, nullptr);
		SetFullViewport(pContext.Get(), float(windowWidth), float(windowHeight));
	}

	if (!fpsTimer.isrunning) { fpsTimer.Start(); } static int fpsCounter = 0; static std::string fpsString = "FPS: 0"; if (++fpsCounter, fpsTimer.GetMilisecondsElapsed() > 1000) { fpsString = "FPS: " + std::to_string(fpsCounter); fpsCounter = 0; fpsTimer.Restart(); }

	auto CameraPos = camera.GetPositionFloat3(); std::string CameraPrint = std::format("X: {:.2f} Y: {:.2f} Z: {:.2f}", CameraPos.x, CameraPos.y, CameraPos.z); auto CameraRot = camera.GetRotationFloat3(); std::string CameraPrintRot = std::format("Pitch: {:.2f} Roll: {:.2f} Yaw: {:.2f}", CameraRot.x, CameraRot.y, CameraRot.z);

	spriteBatch->Begin();
	if (drawrt1)           spriteBatch->Draw(gbufA.rt1_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt0)           spriteBatch->Draw(gbufA.rt0.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawrt2)           spriteBatch->Draw(gbufA.rt2.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_diffuse) spriteBatch->Draw(gbufA.light_diffuse.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_specular)spriteBatch->Draw(gbufA.light_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawLight_ibl)     spriteBatch->Draw(gbufA.light_ibl_specular.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawShading)       spriteBatch->Draw(gbufA.shading_result.srv.Get(), DirectX::XMFLOAT2(0, 0));
	if (drawShadingRead)   spriteBatch->Draw(gbufA.shading_result_read.srv.Get(), DirectX::XMFLOAT2(0, 0));
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(fpsString).c_str(), DirectX::XMFLOAT2(0, 0), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrint).c_str(), DirectX::XMFLOAT2(0, 50), DirectX::Colors::Wheat);
	spriteFont->DrawString(spriteBatch.get(), StringConverter::StringToWide(CameraPrintRot).c_str(), DirectX::XMFLOAT2(0, 100), DirectX::Colors::Wheat);
	spriteBatch->End();

	
	ImGui_ImplDX11_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

{
	ImGuiIO& io = ImGui::GetIO();
	ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
	ImGui::SetNextWindowSize(io.DisplaySize, ImGuiCond_Always);
	ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground |
		ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoFocusOnAppearing;

	ImGui::Begin("Entity Labels", nullptr, flags);
	if (drawEntityLabels){
		
		for (size_t entIdx = 0; entIdx < entitiesToDraw.size(); ++entIdx)
		{
			const auto& e = entitiesToDraw[entIdx];

			if (e.rtype == EntityType::SkyEntity)
				continue;

			if (!IsEntityTypeVisible(e.rtype))
				continue;

			ImVec2 screen;
			if (!WorldToScreen(viewState, glm::vec3(e.pos.x, e.pos.y, e.pos.z),
				io.DisplaySize.x, io.DisplaySize.y, screen))
				continue;

			screen.x += 12.0f;
			screen.y -= 12.0f;
			auto itName = s_nameCache.find(entIdx);
			if (itName == s_nameCache.end()) {
				std::string name;
				if (name.empty()) {
					char tmp[64]; std::snprintf(tmp, sizeof(tmp), " %s 0x%08X", e.name.c_str(), e.id);
					name = tmp;
				}
				itName = s_nameCache.emplace(entIdx, std::move(name)).first;
			}

			char label[256];
			std::snprintf(label, sizeof(label), "%s  (%s)", itName->second.c_str(), EntityTypeName(e.rtype));
			ImGui::PushID((int)entIdx);

			const ImVec2 textSize = ImGui::CalcTextSize(label);

			ImGui::SetCursorScreenPos(screen);
			ImGui::InvisibleButton("##ent_label", textSize);

			const bool hovered = ImGui::IsItemHovered();
			const bool clicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);

			if (clicked) {
				selectedEntityIndex = (int32_t)entIdx;
				selectedEntityId = e.id;
				selectedEntityType = e.rtype;
				selectedEntityPos = { e.pos.x, e.pos.y, e.pos.z };
				selectedEntitySettingsOpen = true;
			}

			const ImU32 col = hovered ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 255, 255, 220);

			ImGui::GetForegroundDrawList()->AddText(screen, col, label);

			ImGui::PopID();
		}

		//
	}
	}
	ImGui::End();
	DrawTfxBytecodeInspectorUI();
	ImGui::Begin("Debug Menu");
	static float value = 50.0f;
	ImGui::Checkbox("Draw gbuffer", &drawrt0);
	ImGui::Checkbox("Draw Rt1", &drawrt1);
	ImGui::Checkbox("Draw Rt2", &drawrt2);
	ImGui::Checkbox("Draw Light_diffuse", &drawLight_diffuse);
	ImGui::Checkbox("Draw Light_specular", &drawLight_specular);
	ImGui::Checkbox("Draw Shading", &drawShading);
	ImGui::Checkbox("Global Lighting", &stageGlobalLighting);
	ImGui::Checkbox("Show Entity Labels", &drawEntityLabels);
	ImGui::Checkbox("Run Ambient Occlusion", &runAmbientOcclusion);
	ImGui::SliderFloat("Lod/View Distance", &lod_distance, 0.0f, 1000.0f);
	if (lod_distance == 1000.0f) {
		lod_distance = INFINITY;
	}
	if (ImGui::CollapsingHeader("Entity Type Filters"))
	{
		auto checkboxType = [](const char* label, EntityType t)
			{
				uint32_t bit = 1u << (uint32_t)t;
				bool enabled = (g_entityTypeVisibleMask & bit) != 0;
				if (ImGui::Checkbox(label, &enabled)) {
					if (enabled) g_entityTypeVisibleMask |= bit;
					else         g_entityTypeVisibleMask &= ~bit;
				}
			};

		if (ImGui::Button("Show All")) {
			g_entityTypeVisibleMask =
				(1u << (uint32_t)EntityType::Standard) |
				(1u << (uint32_t)EntityType::Activity) |
				(1u << (uint32_t)EntityType::ParticleSystem) |
				(1u << (uint32_t)EntityType::Combatant) |
				(1u << (uint32_t)EntityType::SkyEntity) |
				(1u << (uint32_t)EntityType::ChildEntity)|
				(1u << (uint32_t)EntityType::CombatantChild);
		}
		ImGui::SameLine();
		if (ImGui::Button("Hide All")) {
			g_entityTypeVisibleMask = 0;
		}

		checkboxType("Standard", EntityType::Standard);
		checkboxType("Activity", EntityType::Activity);
		checkboxType("ParticleSystem", EntityType::ParticleSystem);
		checkboxType("Combatant", EntityType::Combatant);
		checkboxType("SkyEntity", EntityType::SkyEntity);
		checkboxType("ChildEntity", EntityType::ChildEntity);
		checkboxType("CombatantChild", EntityType::CombatantChild);
	}
	if (ImGui::CollapsingHeader("ViewExtern"))
	{
		ViewExtern v{};
		if (TryGetViewExtern(externs, v))
		{
			ImGui::Text("Res: %.0f x %.0f", v.resolution_width, v.resolution_height);
			ImGui::Text("Pos: (%.3f, %.3f, %.3f, %.3f)", v.position.x, v.position.y, v.position.z, v.position.w);
			ImGui::Text("unk30: (%.3f, %.3f, %.3f, %.3f)", v.unk30.x, v.unk30.y, v.unk30.z, v.unk30.w);

			ShowMat("world_to_camera", v.world_to_camera);
			ShowMat("camera_to_projective", v.camera_to_projective);
			ShowMat("camera_to_world", v.camera_to_world);
			ShowMat("world_to_projective", v.world_to_projective);
			ShowMat("projective_to_world", v.projective_to_world);
			ShowMat("projective_to_camera", v.projective_to_camera);
			ShowMat("target_pixel_to_camera", v.target_pixel_to_camera);
			ShowMat("target_pixel_to_world", v.target_pixel_to_world);
			ShowMat("tptow_no_proj_w", v.tptow_no_proj_w);
		}
		else {
			ImGui::TextDisabled("ViewExtern not set.");
		}
	}
	
	if (ImGui::CollapsingHeader("Atmosphere"))
	{
		if (ImGui::Button("Ensure + Defaults (once)")) {
			EnsureAtmosphereCapacity(externs);
			AtmosphereSetDefaults(externs);
		}
		bool changed = ShowAtmosphereExternEditor(externs);

		
		if (changed) {
			
			externs.Upload(pContext.Get(), TfxExtern::Atmosphere);
		}
	}
	if (ImGui::CollapsingHeader("Frame")) {
		if (ImGui::Button("Ensure + Defaults (once)")) { EnsureFrameCapacity(externs); FrameSetDefaults(externs); }
		bool changed = ShowFrameExternEditor(externs);
		if (changed) externs.Upload(pContext.Get(), TfxExtern::Frame);
	}

	if (ImGui::CollapsingHeader("Deferred")) {
		if (ImGui::Button("Ensure + Defaults (once)")) { EnsureDeferredCapacity(externs); DeferredSetDefaults(externs); }
		bool changed = ShowDeferredExternEditor(externs);
		if (changed) externs.Upload(pContext.Get(), TfxExtern::Deferred);
	}

	if (ImGui::CollapsingHeader("GlobalLighting")) {
		if (ImGui::Button("Ensure + Defaults (once)")) { EnsureGlobalLightingCapacity(externs); GlobalLightingSetDefaults(externs); }
		bool changed = ShowGlobalLightingExternEditor(externs);
		if (changed) externs.Upload(pContext.Get(), TfxExtern::GlobalLighting);
	}
	if (ImGui::CollapsingHeader("Global Channels"))
	{
		bool changed = ShowGlobalChannelsEditor(channels, externs, true);
		if (changed) {
			externs.Upload(pContext.Get(), TfxExtern::Generic);
		}
	}
	ImGui::End();
	ShowEntityChannelEditorUI(entitiesToDraw, camera.GetPositionFloat3());


	if (selectedEntitySettingsOpen && selectedEntityIndex >= 0)
	{
		const RenderEntity* sel = nullptr;
		
		if (selectedEntityIndex >= 0 && selectedEntityIndex < (int32_t)entitiesToDraw.size()) {
			const auto& cand = entitiesToDraw[(size_t)selectedEntityIndex];
			if (cand.id == selectedEntityId && cand.rtype == selectedEntityType) sel = &cand;
		}
		
		if (!sel) {
			float bestD2 = FLT_MAX;
			int32_t bestIdx = -1;
			for (size_t i = 0; i < entitiesToDraw.size(); ++i) {
				const auto& e = entitiesToDraw[i];
				if (e.id != selectedEntityId || e.rtype != selectedEntityType) continue;
				const float dx = e.pos.x - selectedEntityPos.x;
				const float dy = e.pos.y - selectedEntityPos.y;
				const float dz = e.pos.z - selectedEntityPos.z;
				const float d2 = dx*dx + dy*dy + dz*dz;
				if (d2 < bestD2) { bestD2 = d2; bestIdx = (int32_t)i; }
			}
			if (bestIdx >= 0) {
				selectedEntityIndex = bestIdx;
				sel = &entitiesToDraw[(size_t)bestIdx];
			}
		}

		ImGui::Begin("Entity Settings", &selectedEntitySettingsOpen);

		if (!sel) {
			ImGui::TextDisabled("Selected entity (0x%08X) not found in current draw list.", selectedEntityId);
			ImGui::End();
			return;
		}

		ImGui::Text("Name: %s", sel->name.c_str());
		ImGui::SameLine();
		ImGui::Text("Type: %s", EntityTypeName(sel->rtype));
		ImGui::Separator();
		ImGui::Text("Pos: (%.3f, %.3f, %.3f)  Scale: %.3f",
			sel->pos.x, sel->pos.y, sel->pos.z, sel->pos.w);

		// Validate index once
		bool validIndex =
			(selectedEntityIndex >= 0) &&
			(selectedEntityIndex < (int)entitiesToDraw.size());

		if (!validIndex) {
			ImGui::TextDisabled("SelectedEntityIndex is out of range.");
			ImGui::End();
			return;
		}

		auto& e = entitiesToDraw[(size_t)selectedEntityIndex];

	
		if (!(e.id == selectedEntityId && e.rtype == selectedEntityType)) {
			ImGui::TextDisabled("Selected entity does not match draw-list entry.");
			ImGui::End();
			return;
		}

		ImGui::SeparatorText("Transform");

		float pos[3] = { e.pos.x, e.pos.y, e.pos.z };
		float scale = e.pos.w;

		if (ImGui::DragFloat3("Position (XYZ)", pos, 0.05f)) {
			e.pos.x = pos[0];
			e.pos.y = pos[1];
			e.pos.z = pos[2];
			selectedEntityPos = { e.pos.x, e.pos.y, e.pos.z };
			e.cb1_single = BuildCB1FromEntity(e);
			e.occlusion_bounds = Aabb::FromCenterExtents(e.meshData.model_offset + e.pos, e.meshData.model_scale* e.pos.w);
		}

		if (ImGui::DragFloat("Scale", &scale, 0.01f, 0.0f, 10000.0f)) {
			e.pos.w = scale;
			e.cb1_single = BuildCB1FromEntity(e);
			e.occlusion_bounds = Aabb::FromCenterExtents(e.meshData.model_offset + e.pos, e.meshData.model_scale * e.pos.w);
		}

		if (ImGui::Button("Reset")) {
			e.pos = e.base_placement_pos;
			e.cb1_single = BuildCB1FromEntity(e);
			e.occlusion_bounds = Aabb::FromCenterExtents(e.meshData.model_offset + e.pos, e.meshData.model_scale * e.pos.w);
		}

		ImGui::Text("Channels: %zu", sel->channels.size());
		ImGui::SeparatorText("Channel Overrides");

		for (auto& kv : e.channels) {
			float v4[4] = { kv.second.x, kv.second.y, kv.second.z, kv.second.w };
			char clabel[64];
			std::snprintf(clabel, sizeof(clabel), "0x%08X", kv.first);

			if (ImGui::DragFloat4(clabel, v4, 0.01f)) {
				kv.second = Vec4(v4[0], v4[1], v4[2], v4[3]);
			}
		}

		ImGui::SeparatorText("Material Index");

		int maxIndex = 0;

		for (const auto& entry : e.external_material_mapping) {
			if(entry.technique_count-1 > maxIndex) {
				maxIndex = entry.technique_count;
			}
		}
			
		int idx = (int)e.varient_index;
		if (idx < 0) idx = 0;
		if (idx > maxIndex) idx = maxIndex;

		if (ImGui::DragInt("VarientShaderIndex", &idx, 0.1f, 0, maxIndex)) {
			e.varient_index = (uint16_t)idx;
		}

		ImGui::End();
			
	}

	ImGui::Begin("Activity Selector");
	if (ImGui::CollapsingHeader("Activities & Maps"))
	{
		ActivityProvider provider = [&]() -> const std::vector<ActivityDef>&{
			static std::vector<ActivityDef> cache;
			cache.clear();
			cache.reserve(this->activities.size());

			for (const TigerActivity& ta : this->activities) {
				ActivityDef a{};
				a.display_name = ta.dev_name;
				a.activity_id = ta.id;


				a.maps.reserve(ta.bubbles.size());
				for (const auto& tm : ta.bubbles) {
					
					a.maps.push_back(MapDef{ tm.name.c_str(), tm.parent_hash, tm.bubble_hash });
				}

				
				a.phases.reserve(ta.phases.size());
				for (const auto& tp : ta.phases) {
					a.phases.push_back(PhaseDef{
						tp.name,   
						tp.parent_hash,    
						tp.bubble_hash
						});
				}

				cache.emplace_back(std::move(a));
			}

			return cache;
			};

		ActivityBrowserCallbacks cbs;

		cbs.on_map_phase_chosen = [&](const ActivityDef& act,
			const MapDef& map,
			const PhaseDef& phase,
			const bool loadCombatant)
			{
				char buf[256];
				std::snprintf(buf, sizeof(buf),
					"Chosen '%s' | Map: %s (0x%08X) | Phase: %s (0x%08X)\n",
					act.display_name.c_str(),
					map.display_name.c_str(),
					map.map_hash,
					phase.display_name.c_str(),
					phase.phase_tag);
				
				this->s_nameCache.clear();
				this->staticsToDraw.clear();
				this->lightsToDraw.clear();
				this->entitiesToDraw.clear();
				this->terrainToDraw.clear();
				dynamicPartCache_.clear();
				loadzone = std::make_unique<LoadZone>(*this);
				loadzone->parentHash = map.map_hash;   

				loadzone->ProcessMap(); 

				loadzone->load_activity_phase(TagHash(phase.phase_tag), loadCombatant);
				this->staticsToDraw = loadzone->statics;
				this->lightsToDraw = loadzone->lights;
				this->entitiesToDraw = loadzone->entities;
				this->staticAO1 = loadzone->AOMap1;
				this->terrainToDraw = loadzone->terrain_patches;
			};

		cbs.on_map_chosen = [&](const ActivityDef& act, const MapDef& map, bool loadCombatant) {
			this->s_nameCache.clear();
			this->staticsToDraw.clear();
			this->lightsToDraw.clear();
			this->entitiesToDraw.clear();
			this->terrainToDraw.clear();

			loadzone = std::make_unique<LoadZone>(*this);
			loadzone->parentHash = map.map_hash;
			loadzone->ProcessMap();
			dynamicPartCache_.clear();
			this->staticsToDraw = loadzone->statics;
			this->lightsToDraw = loadzone->lights;
			this->entitiesToDraw = loadzone->entities;
			this->staticAO1 = loadzone->AOMap1;
			this->terrainToDraw = loadzone->terrain_patches;
			};

		cbs.on_load_all_activity_phases = [&](const ActivityDef& act, const MapDef& map, bool loadCombatant) {
			this->s_nameCache.clear();
			this->staticsToDraw.clear();
			this->lightsToDraw.clear();
			this->entitiesToDraw.clear();
			this->terrainToDraw.clear();

			loadzone = std::make_unique<LoadZone>(*this);
			loadzone->parentHash = map.map_hash;
			loadzone->ProcessMap();
			dynamicPartCache_.clear();
			for (const auto& phase : act.phases) {
				if (phase.bubble_hash != map.map_name_hash) { continue; }
				loadzone->load_activity_phase(TagHash(phase.phase_tag),loadCombatant);
			}
			this->staticsToDraw = loadzone->statics;
			this->lightsToDraw = loadzone->lights;
			this->entitiesToDraw = loadzone->entities;
			this->staticAO1 = loadzone->AOMap1;
			this->terrainToDraw = loadzone->terrain_patches;
			};
		DrawActivityBrowser(provider, cbs);
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
	printf("Starting Initialize DirectX...\n");
	if (!InitializeDirectX(hWnd))
		return false;
	printf("DirectX initialized.\n");
	if (!InitializeShaders())
		return false;
	printf("Shaders initialized.\n");
	if (!InitializeRenderGlobals())
		return false;
	if (!InitializeScene())
		return false;
	printf("Scene initialized.\n");

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
		rvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;   
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

		
		CD3D11_DEPTH_STENCIL_DESC dsRZ(D3D11_DEFAULT);
		dsRZ.DepthEnable = TRUE;
		dsRZ.StencilEnable = FALSE;
		dsRZ.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		dsRZ.StencilReadMask = 0;
		dsRZ.StencilWriteMask = 0;
		dsRZ.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
		hr = pDevice->CreateDepthStencilState(&dsRZ, depthStencilState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		CD3D11_DEPTH_STENCIL_DESC dsRZ1(D3D11_DEFAULT);
		dsRZ1.DepthEnable = FALSE;
		dsRZ1.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsRZ1.DepthFunc = D3D11_COMPARISON_NEVER;
		hr = pDevice->CreateDepthStencilState(&dsRZ1, depthStencilStateLighting.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		D3D11_DEPTH_STENCIL_DESC dd = {};
		dd.DepthEnable = FALSE;
		pDevice->CreateDepthStencilState(&dd, dsDisabled.GetAddressOf());

		
		pContext->OMSetDepthStencilState(depthStencilState.Get(), 0);

		CD3D11_RASTERIZER_DESC rsDesc(D3D11_DEFAULT);
		rsDesc.CullMode = D3D11_CULL_BACK;
		rsDesc.FrontCounterClockwise = TRUE;
		hr = pDevice->CreateRasterizerState(&rsDesc, rasterizerState.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer.");

		CD3D11_RASTERIZER_DESC rsDescNoCull = rsDesc;
		rsDescNoCull.CullMode = D3D11_CULL_NONE;
		hr = pDevice->CreateRasterizerState(&rsDescNoCull, rasterizerStateNoCull.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create rasterizer (nocull).");



{
	CD3D11_RASTERIZER_DESC wfDesc(rsDesc);
	wfDesc.FillMode = D3D11_FILL_WIREFRAME;
	wfDesc.CullMode = D3D11_CULL_NONE;
	wfDesc.FrontCounterClockwise = TRUE;
	HRESULT hrwf = pDevice->CreateRasterizerState(&wfDesc, rasterizerStateWireframe.GetAddressOf());
	COM_ERROR_IF_FAILED(hrwf, "Failed to create wireframe rasterizer.");
}


{
	CD3D11_DEPTH_STENCIL_DESC dsRO(D3D11_DEFAULT);
	dsRO.DepthEnable = TRUE;
	dsRO.StencilEnable = FALSE;
	dsRO.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
	dsRO.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL;
	HRESULT hrro = pDevice->CreateDepthStencilState(&dsRO, depthStencilReadOnly.GetAddressOf());
	COM_ERROR_IF_FAILED(hrro, "Failed to create depthStencilReadOnly.");
}

		std::filesystem::path font_path = "entropy.spritefont";
		spriteBatch = std::make_unique<DirectX::SpriteBatch>(this->pContext.Get());
		spriteFont = std::make_unique<DirectX::SpriteFont>(
			this->pDevice.Get(),
			font_path.c_str());

		CD3D11_SAMPLER_DESC samplerDesc(D3D11_DEFAULT);
		samplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		samplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;

		hr = this->pDevice->CreateSamplerState(&samplerDesc, this->samplerState.GetAddressOf());

		D3D11_BLEND_DESC bd_op = {};
		bd_op.AlphaToCoverageEnable = FALSE;
		bd_op.IndependentBlendEnable = TRUE;

		for (int i = 0; i < 8; ++i)
		{
			D3D11_RENDER_TARGET_BLEND_DESC& rt = bd_op.RenderTarget[i];
			rt.BlendEnable = FALSE;                    
			rt.SrcBlend = D3D11_BLEND_ONE;
			rt.DestBlend = D3D11_BLEND_ZERO;
			rt.BlendOp = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}

		hr = pDevice->CreateBlendState(&bd_op, bsOpaque.GetAddressOf());

		COM_ERROR_IF_FAILED(hr, "Failed to create device sampler state.");
		D3D11_BLEND_DESC bd = {};
		bd.AlphaToCoverageEnable = FALSE;
		bd.IndependentBlendEnable = TRUE; 

		for (int i = 0; i < 8; ++i)
		{
			D3D11_RENDER_TARGET_BLEND_DESC rt = {};
			rt.BlendEnable = FALSE; 
			rt.SrcBlend = D3D11_BLEND_ONE;
			rt.DestBlend = D3D11_BLEND_ZERO;
			rt.BlendOp = D3D11_BLEND_OP_ADD;
			rt.SrcBlendAlpha = D3D11_BLEND_ONE;
			rt.DestBlendAlpha = D3D11_BLEND_ZERO;
			rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
			rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL; 
			bd.RenderTarget[i] = rt;
		}
		hr = pDevice->CreateBlendState(&bd, bsGBufferOpaqueIndependent.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create bsGBufferOpaqueIndependent");

		CD3D11_RASTERIZER_DESC rzGBuf(D3D11_DEFAULT);
		rzGBuf.CullMode = D3D11_CULL_BACK;
		rzGBuf.FrontCounterClockwise = TRUE;  
		rzGBuf.DepthBias = 0;                
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

	
		{
			CD3D11_SAMPLER_DESC sd(D3D11_DEFAULT);
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			pDevice->CreateSamplerState(&sd, samplerLinearClamp.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sd(D3D11_DEFAULT);
			sd.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sd.AddressU = sd.AddressV = sd.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sd.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&sd, this->shading1.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC s(D3D11_DEFAULT);
			s.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;   
			s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			s.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&s, shading2.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC sl(D3D11_DEFAULT);
			sl.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
			sl.AddressU = sl.AddressV = sl.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			sl.MipLODBias = 0.0f;
			pDevice->CreateSamplerState(&sl, lighting1.GetAddressOf());
		}
		{
			CD3D11_SAMPLER_DESC s(D3D11_DEFAULT);
			s.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;   
			s.AddressU = s.AddressV = s.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
			s.MipLODBias = -0.5f;
			pDevice->CreateSamplerState(&s, lighting2.GetAddressOf());
		}

		{
			D3D11_BLEND_DESC bd{};
			bd.RenderTarget[0].BlendEnable = FALSE;
			bd.RenderTarget[0].SrcBlend = D3D11_BLEND_DEST_COLOR; 
			bd.RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
			bd.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
			bd.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
			bd.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
			pDevice->CreateBlendState(&bd, bsMultiply.GetAddressOf());
		}

		{
			CD3D11_RASTERIZER_DESC rs(D3D11_DEFAULT);
			rs.CullMode = D3D11_CULL_FRONT;    
			rs.FrontCounterClockwise = TRUE;   
			pDevice->CreateRasterizerState(&rs, rasterizerCullFront.GetAddressOf());
		}

		{
			CD3D11_RASTERIZER_DESC rz(D3D11_DEFAULT);
			rz.FillMode = D3D11_FILL_SOLID;
			rz.CullMode = D3D11_CULL_BACK;
			rz.FrontCounterClockwise = TRUE;      
			rz.DepthBias = 5;                     
			rz.DepthBiasClamp = 1.0e10f;          
			rz.SlopeScaledDepthBias = 2.0f;       
			rz.DepthClipEnable = TRUE;            
			rz.ScissorEnable = FALSE;            
			rz.MultisampleEnable = FALSE;         
			rz.AntialiasedLineEnable = FALSE;    

			HRESULT hr = pDevice->CreateRasterizerState(&rz, rasterizerStateBiased.GetAddressOf());
			COM_ERROR_IF_FAILED(hr, "Failed to create biased rasterizer state.");
		}

		{
			D3D11_BLEND_DESC bd{};
			bd.AlphaToCoverageEnable = FALSE;
			bd.IndependentBlendEnable = TRUE;

			for (int i = 0; i < 2; ++i) {
				D3D11_RENDER_TARGET_BLEND_DESC rt{};
				rt.BlendEnable = TRUE;
				rt.SrcBlend = D3D11_BLEND_ONE;
				rt.DestBlend = D3D11_BLEND_ONE;
				rt.BlendOp = D3D11_BLEND_OP_ADD;
				rt.SrcBlendAlpha = D3D11_BLEND_ONE;
				rt.DestBlendAlpha = D3D11_BLEND_ONE;
				rt.BlendOpAlpha = D3D11_BLEND_OP_ADD;
				rt.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
				bd.RenderTarget[i] = rt;
			}
		
			pDevice->CreateBlendState(&bd, bsAdditive2RT.GetAddressOf());
		}
		D3D11_DEPTH_STENCILOP_DESC dsdesc{};
		dsdesc.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsdesc.StencilFunc = D3D11_COMPARISON_ALWAYS;


		{
			CD3D11_DEPTH_STENCIL_DESC ds(D3D11_DEFAULT);
			ds.DepthEnable = FALSE;
			ds.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;        
			ds.DepthFunc = D3D11_COMPARISON_ALWAYS;          
			ds.StencilReadMask = 0;
			ds.StencilWriteMask = 0;
			ds.BackFace = dsdesc;
			ds.FrontFace = dsdesc;
			pDevice->CreateDepthStencilState(&ds, depthStencilLightVolume.GetAddressOf());
		}

		ComPtr<ID3D11Texture2D> backbuf;
		DXGI_SWAP_CHAIN_DESC scd1 = {}; pSwapChain->GetDesc(&scd1);
		pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)backbuf.GetAddressOf());


		CD3D11_DEPTH_STENCIL_DESC ds_decal(D3D11_DEFAULT);
		ds_decal.DepthEnable = TRUE;
		ds_decal.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		ds_decal.DepthFunc = D3D11_COMPARISON_GREATER_EQUAL; 
		ds_decal.StencilEnable = FALSE;
		ds_decal.StencilReadMask = 0;
		ds_decal.StencilWriteMask = 0;
		hr = pDevice->CreateDepthStencilState(&ds_decal, depthStencilDecal.GetAddressOf());
		COM_ERROR_IF_FAILED(hr, "Failed to create reversed-Z depth stencil state.");

		D3D11_RENDER_TARGET_VIEW_DESC rtvSRGB{};
		rtvSRGB.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvSRGB.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		pDevice->CreateRenderTargetView(backbuf.Get(), &rtvSRGB, pRenderTargetView.GetAddressOf());


		D3D11_RENDER_TARGET_VIEW_DESC rtvUNORM{};
		rtvUNORM.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
		rtvUNORM.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		pDevice->CreateRenderTargetView(backbuf.Get(), &rtvUNORM, pRenderTargetViewLinear.GetAddressOf());
	}


	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}

	pool = std::make_unique<ThreadPool>();
	mainQueue = std::make_unique<MainThreadQueue>();
	registry = std::make_unique<RuntimeAssetRegistry>();
	assets = std::make_unique<AssetSystem>(pDevice.Get(), pContext.Get(), *pool, *mainQueue, registry.get());
	
	CreateTerrainCB64();
	CreateScopeViewCB12(pDevice.Get());
	CreateInstanceBuffer();
	CreateCB1(pDevice.Get());
	CreateCB1_FallBack(pDevice.Get());
	if (SUCCEEDED(pContext.As(&mt)) && mt) mt->SetMultithreadProtected(TRUE);
	PublishGlobalChannelsToExterns(externs, channels);
	RenderStates::Create(pDevice.Get(), states);

	return true;
}

bool Graphics::InitializeShaders()
{
	Microsoft::WRL::ComPtr<ID3DBlob> vsBytecode;
	HRESULT hr = CompileVSFromMemory(
		pDevice.Get(),
		kVS_SkinnedHack,
		"VSMain",
		"vs_5_0",
		&entity_vs_override,
		vsBytecode.GetAddressOf());
	if (FAILED(hr)) {
		ErrorLogger::Log(hr, "VS compile failed");
		
	}
	return true;
}

void Graphics::InitializeScopes()
{
	auto& scopes = GlobalData::getScopes();
	for (auto& scope : scopes)
	{
		std::pair<std::string, TigerScope> value;
		TigerScope scope_obj{};
		value.first = scope.first;
		printf("Loading Scope : %08X \n", scope.second.hash);
		auto ScopeTag = bin::parse<SScope>(scope.second.data, scope.second.size, bin::Endian::Little);
		scope_obj.Scope = ScopeTag;
		scope_obj.LoadScopeInfo(pDevice, pContext);
		value.second = scope_obj;
		this->scopes.push_back(value);
	}
}

void Graphics::LoadGlobalTextures() {
	auto GlobalTex = GlobalData::getGlobalTextures();
	for (auto tex : GlobalTex) {
		Microsoft::WRL::ComPtr<ID3D11Texture2D> textureLoaded;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> texSrv;
		auto payload = BuildTexturePayloadFromTag(tex.second);
		D3D11_SHADER_RESOURCE_VIEW_DESC sd;
		sd.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		sd.Texture2D.MostDetailedMip = 0;
		sd.Texture2D.MipLevels = payload->desc.MipLevels;
		sd.Format = payload->desc.Format;
		HRESULT hr = pDevice->CreateTexture2D(&payload->desc, payload->subresources.data(), &textureLoaded);
		hr = pDevice->CreateShaderResourceView(textureLoaded.Get(), &sd, &texSrv);
		this->global_textures[tex.first] = texSrv;
	}
	HRESULT hr = LoadEmbeddedTextureSRV(pDevice.Get(), 102,false, this->temp_angle_lookup.GetAddressOf());
	if (FAILED(hr))
		ErrorLogger::Log(hr, "Failed to load baked texture1");
	hr = LoadEmbeddedTextureSRV(pDevice.Get(), IDB_SKY,false, this->sky_hemisphere_lookup.GetAddressOf());
	if (FAILED(hr))
		ErrorLogger::Log(hr, "Failed to load baked texture2");
}

bool Graphics::InitializeScene()
{
	try {
		camera.SetPosition(0.0f, 0.0f, 0.0f);
		camera.SetProjectionValues(120.0f, static_cast<float>(windowWidth) / static_cast<float>(windowHeight), 0.01f, 1.0f);
	}
	catch (COMException& exception)
	{
		ErrorLogger::Log(exception);
		return false;
	}
	InitializeInputLayouts();
	Create1x1SRV(0xFFFFFFFF, white1x1SRV);
	Create1x1SRV(0xFF808080, grey1x1SRV);
	InitializeScopes();
	LoadGlobalTextures();
	CreateLightVolumeResources();
	CreateInstanceBuffer();
	InitAnnotation();
	this->activities = GlobalData::globalActivities();
	loadzone = std::make_unique<LoadZone>(*this);

	auto e_to_load = TagHash(0x810A8296);
	Aabb a;
	loadzone->load_entity_into_scene(e_to_load, glm::quat(), glm::vec4(0.0f,0.0f,0.0f,1.0f));
	
	if (loadzone) {
		this->staticsToDraw = loadzone->statics;
		this->lightsToDraw = loadzone->lights;
		this->entitiesToDraw = loadzone->entities;
		this->staticAO1 = loadzone->AOMap1;
	}
	EnsureCB1_StaticReusable(pDevice.Get(), g_cb1);
	printf("Loading Render Engine\n");
	
	gTimer.reset();
	return true;
}