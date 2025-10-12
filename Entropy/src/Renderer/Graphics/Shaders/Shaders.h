#pragma once
#include "Renderer/Tools/ErrorLogger.h"
#include "TigerEngine/tag.h"
#pragma comment(lib, "D3DCompiler.lib")
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>


class VertexShader
{
public:
	bool Initialize(Microsoft::WRL::ComPtr<ID3D11Device> &device, std::wstring shaderpath, D3D11_INPUT_ELEMENT_DESC * layoutDesc, UINT numElements);
	bool InitializeBlob(Microsoft::WRL::ComPtr<ID3D11Device>& device, std::wstring shaderpath, D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements);
	ID3D11VertexShader* GetShader();
	ID3D10Blob* GetBuffer();
	ID3D11InputLayout* GetInputLayout();
private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Microsoft::WRL::ComPtr<ID3D10Blob> shader_buffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};

class PixelShader
{
public:
	bool Initialize(Microsoft::WRL::ComPtr<ID3D11Device>& device, std::wstring shaderpath);
	ID3D11PixelShader* GetShader();
	ID3D10Blob* GetBuffer();
private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader = nullptr;
	Microsoft::WRL::ComPtr<ID3D10Blob> shader_buffer = nullptr;
};

class D2VertexShader
{
public:
	bool Initialize(ID3D11Device* device,D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements);
	bool InitializeBlob(Microsoft::WRL::ComPtr<ID3D11Device>& device,D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements);
	ID3D11VertexShader* GetShader();
	ID3D10Blob* GetBuffer();
	ID3D11InputLayout* GetInputLayout();
	TagHash tag;
	std::vector<TagHash> vs_texs;
	void SetShader(Microsoft::WRL::ComPtr<ID3D11PixelShader> newShader);
private:
	Microsoft::WRL::ComPtr<ID3D11VertexShader> shader;
	Microsoft::WRL::ComPtr<ID3D10Blob> shader_buffer;
	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
};

class D2PixelShader
{
public:
	bool Initialize(ID3D11Device* device);
	ID3D11PixelShader* GetShader();
	ID3D10Blob* GetBuffer();
	TagHash tag;
	std::vector<TagHash> ps_texs;
private:
	Microsoft::WRL::ComPtr<ID3D11PixelShader> shader = nullptr;
	Microsoft::WRL::ComPtr<ID3D10Blob> shader_buffer = nullptr;
};