#include "Shaders.h"

bool VertexShader::Initialize(Microsoft::WRL::ComPtr<ID3D11Device> &device, std::wstring shaderpath, D3D11_INPUT_ELEMENT_DESC * layoutDesc, UINT numElements) {
	HRESULT hr = D3DReadFileToBlob(shaderpath.c_str(), shader_buffer.GetAddressOf());
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to read vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	hr = device->CreateVertexShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to create vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	hr = device->CreateInputLayout(
		layoutDesc,
		numElements,
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		inputLayout.GetAddressOf()
	);

	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to create input layout for vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	return true;
}

ID3D11VertexShader* VertexShader::GetShader() {
	return shader.Get();
}

ID3D10Blob* VertexShader::GetBuffer() {
	return shader_buffer.Get();
}

ID3D11InputLayout* VertexShader::GetInputLayout() {
	return inputLayout.Get();
}

bool VertexShader::InitializeBlob(Microsoft::WRL::ComPtr<ID3D11Device>& device, std::wstring shaderpath, D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements) {
	HRESULT hr = D3DReadFileToBlob(shaderpath.c_str(), shader_buffer.GetAddressOf());
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to read vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	hr = device->CreateVertexShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to create vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	hr = device->CreateInputLayout(
		layoutDesc,
		numElements,
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		inputLayout.GetAddressOf()
	);

	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to create input layout for vertex shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	return true;
}

bool D2VertexShader::InitializeBlob(Microsoft::WRL::ComPtr<ID3D11Device>& device, D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements) {
	HRESULT hr = device->CreateVertexShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	hr = device->CreateInputLayout(
		layoutDesc,
		numElements,
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		inputLayout.GetAddressOf()
	);
	return true;
}


bool PixelShader::Initialize(Microsoft::WRL::ComPtr<ID3D11Device>& device, std::wstring shaderpath) {
	HRESULT hr = D3DReadFileToBlob(shaderpath.c_str(), shader_buffer.GetAddressOf());
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to read pixel shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}
	hr = device->CreatePixelShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	if (FAILED(hr)) {
		std::wstring errorMsg = L"Failed to create pixel shader file: " + shaderpath;
		ErrorLogger::Log(hr, errorMsg);
		return false;
	}

	return true;
}

ID3D11PixelShader* PixelShader::GetShader() {
	return shader.Get();
}

ID3D10Blob* PixelShader::GetBuffer() {
	return shader_buffer.Get();
}


bool D2VertexShader::Initialize(Microsoft::WRL::ComPtr<ID3D11Device>& device, D3D11_INPUT_ELEMENT_DESC* layoutDesc, UINT numElements) {
	HRESULT hr = device->CreateVertexShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	hr = device->CreateInputLayout(
		layoutDesc,
		numElements,
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		inputLayout.GetAddressOf()
	);
	return true;
}

ID3D11VertexShader* D2VertexShader::GetShader() {
	return shader.Get();
}

ID3D10Blob* D2VertexShader::GetBuffer() {
	return shader_buffer.Get();
}

ID3D11InputLayout* D2VertexShader::GetInputLayout() {
	return inputLayout.Get();
}

bool D2PixelShader::Initialize(Microsoft::WRL::ComPtr<ID3D11Device>& device) {
	HRESULT hr = device->CreatePixelShader(
		shader_buffer->GetBufferPointer(),
		shader_buffer->GetBufferSize(),
		NULL,
		shader.GetAddressOf()
	);
	return true;
}

ID3D11PixelShader* D2PixelShader::GetShader() {
	return shader.Get();
}

ID3D10Blob* D2PixelShader::GetBuffer() {
	return shader_buffer.Get();
}