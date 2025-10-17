#ifndef VertexBuffer_h__
#define VertexBuffer_h__
#include <d3d11.h>
#include <wrl/client.h>
#include <memory>
#include "Renderer/Tools/ErrorLogger.h"


struct VertexBufferHeader {
	uint32_t dataSize;
	uint16_t stride;
	uint16_t vType;
};

class D2VertexBuffer
{
public:
	VertexBufferHeader header;
	unsigned char* data;
	uint32_t TagInt;
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	UINT bufferSize = 0;
	Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> srv;
public:
	ID3D11Buffer* Get()const
	{
		return buffer.Get();
	}

	ID3D11ShaderResourceView* GetSRV()const
	{
		return srv.Get();
	}


	ID3D11Buffer* const* GetAddressOf()const
	{
		return buffer.GetAddressOf();
	}

	UINT BufferSize() const
	{
		return bufferSize;
	}
	void InitializeData(TagHash headerTag)
	{
		header = bin::parse<VertexBufferHeader>(headerTag.data, headerTag.size, bin::Endian::Little);
		data = TagHash(headerTag.reference).data;
		bufferSize = static_cast<UINT>(header.dataSize);
	}
	void Initialize(ID3D11Device* device)
	{
		if (buffer.Get() != nullptr)
			buffer.Reset();
		try
		{
			D3D11_BUFFER_DESC vertexBufferDesc;
			ZeroMemory(&vertexBufferDesc, sizeof(vertexBufferDesc));

			vertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			vertexBufferDesc.ByteWidth = header.dataSize;
			vertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vertexBufferDesc.CPUAccessFlags = 0;
			vertexBufferDesc.MiscFlags = 0;
			vertexBufferDesc.StructureByteStride = 0;

			D3D11_SUBRESOURCE_DATA vertexBufferData;
			ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
			vertexBufferData.pSysMem = data;

			HRESULT hr = device->CreateBuffer(&vertexBufferDesc, &vertexBufferData, buffer.GetAddressOf());
			COM_ERROR_IF_FAILED(hr, "Failed to create D2VertexBuffer");
			std::string name = "VertexBuffer_" + std::to_string(TagInt);
			buffer->SetPrivateData(WKPDID_D3DDebugObjectName,
				static_cast<UINT>(name.size()),
				name.c_str());
			COM_ERROR_IF_FAILED(hr, "Failed to write debug name for VertexBuffer");
			printf("Initialized D2Vertex Buffer - %08X \n", this->TagInt);

		}
		catch (COMException& exception)
		{
			ErrorLogger::Log(exception);
		}
		return;
	}
	void InitializeAsSRV(ID3D11Device* device)
	{
		if (this->header.stride == 1 || this->header.stride == 4)
		{
			D3D11_SUBRESOURCE_DATA vertexBufferData;
			ZeroMemory(&vertexBufferData, sizeof(vertexBufferData));
			vertexBufferData.pSysMem = data;

			D3D11_BUFFER_DESC bd = {};
			bd.ByteWidth = header.dataSize;
			bd.Usage = D3D11_USAGE_DEFAULT;     // or DYNAMIC if you Map/Write
			bd.BindFlags = D3D11_BIND_SHADER_RESOURCE;
			bd.CPUAccessFlags = 0;                       // DYNAMIC -> D3D11_CPU_ACCESS_WRITE
			bd.MiscFlags = 0;                       // leave 0 for typed buffers
			device->CreateBuffer(&bd, &vertexBufferData, &buffer);

			UINT numElements = this->header.dataSize / this->header.stride;  // assumes ByteWidth % stride == 0

			D3D11_SHADER_RESOURCE_VIEW_DESC srvDescVS = {};
			srvDescVS.Format = (this->header.stride == 1)
				? DXGI_FORMAT_R8_UNORM
				: DXGI_FORMAT_R8G8B8A8_UNORM;          // 4 bytes -> float4(unorm) in HLSL
			srvDescVS.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDescVS.Buffer.FirstElement = 0;
			srvDescVS.Buffer.NumElements = numElements;

			HRESULT hr = device->CreateShaderResourceView(buffer.Get(), &srvDescVS, &srv);
			COM_ERROR_IF_FAILED(hr, "Failed to CreateShaderResourceView");
		}
	}
};



#endif