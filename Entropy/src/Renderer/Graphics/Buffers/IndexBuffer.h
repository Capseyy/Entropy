#ifndef IndicesBuffer_h__
#define IndicesBuffer_h__
#include <d3d11.h>
#include <wrl/client.h>
#include <vector>
#include <dxgidebug.h> 
#include "Renderer/Tools/ErrorLogger.h"

struct IndexBufferHeader {
	int8_t unk0;
	int8_t is32;
	uint16_t unk02;
	uint32_t _zeros;
	uint64_t dataSize;
};

class D2IndexBuffer
{
public:
	IndexBufferHeader header;
	unsigned char* data;
	uint32_t TagInt;
private:
	Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
	UINT bufferSize = 0;
public:

	ID3D11Buffer* Get()const
	{
		return buffer.Get();
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
		header = bin::parse<IndexBufferHeader>(headerTag.data, headerTag.size, bin::Endian::Little);
		data = TagHash(headerTag.reference).data;
		bufferSize = static_cast<UINT>(header.dataSize);
	}
	void Initialize(ID3D11Device* device)
	{
		if (buffer.Get() != NULL) {
			buffer.Reset();
		}
		try
		{
			printf("Initialising D2Index Buffer - %08X \n", this->TagInt);

			D3D11_BUFFER_DESC indexBufferDesc;
			ZeroMemory(&indexBufferDesc, sizeof(indexBufferDesc));
			indexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
			indexBufferDesc.ByteWidth = header.dataSize;
			indexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;
			indexBufferDesc.CPUAccessFlags = 0;
			indexBufferDesc.MiscFlags = 0;

			D3D11_SUBRESOURCE_DATA indexBufferData;
			indexBufferData.pSysMem = data;

			HRESULT hr = device->CreateBuffer(&indexBufferDesc, &indexBufferData, buffer.GetAddressOf());
			COM_ERROR_IF_FAILED(hr, "Failed to create D2IndexBuffer");
			std::string name = "IndexBuffer_" + std::to_string(TagInt);
			buffer->SetPrivateData(WKPDID_D3DDebugObjectName,
				static_cast<UINT>(name.size()),
				name.c_str());
			COM_ERROR_IF_FAILED(hr, "Failed to write debug name for index buffer");
			printf("Initialized D2Index Buffer - %08X \n", this->TagInt);
		}
		catch (COMException& exception)
		{
			ErrorLogger::Log(exception);
		}
		return;
	}



};



#endif