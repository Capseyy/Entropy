#pragma once
#include "TigerEngine/Map/static.h"
#include "TigerEngine/tag.h"
#include "TigerEngine/Technique/input_layout.h"
#include "TigerEngine/Technique/technique.h"
#include "Renderer/Graphics/Shaders/Vertex.h"
#include "Renderer/Graphics/Buffers/VertexBuffer.h"
#include "Renderer/Graphics/Buffers/IndexBuffer.h"
#include "Renderer/Graphics/Buffers/ConstantBuffer.h"
#include "Renderer/Graphics/Shaders/material.h"

using namespace DirectX;

struct StaticBuffers {
	D2IndexBuffer indexBuffer;
	D2VertexBuffer vertexBuffer;
	D2VertexBuffer uvBuffer;
	D2VertexBuffer vertexColourBuffer;

	StaticBuffers() = default;
};

struct StaticPart {
	uint32_t index_start;
	uint32_t index_count;
	uint8_t buffer_index;
	uint8_t LodCatagory;
	uint8_t PrimitiveType;
	STechnique material;
	uint8_t input_layout_index;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	Microsoft::WRL::ComPtr<ID3D11VertexShader> vertexShader;
	Microsoft::WRL::ComPtr<ID3D11PixelShader> pixelShader;
	Material materialRender;
};

class StaticRenderer
{
public:
	bool Initialize(uint32_t staticTag);
	void Process();
	bool InitializeRender(ID3D11Device* device, ID3D11DeviceContext* deviceContext);
	TagHash static_tag;
	std::vector<StaticBuffers> buffers;
	std::vector< StaticPart> parts;

private:
	void UpdateWorldMatrix();

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;
	ConstantBuffer<CB_VS_vertexshader>* cb_vs_vertexshader = nullptr;
	ID3D11ShaderResourceView* texture = nullptr;

	D2IndexBuffer vertexBuffer;
	D2IndexBuffer indexBuffer;

	XMMATRIX worldMatrix = XMMatrixIdentity();
};

