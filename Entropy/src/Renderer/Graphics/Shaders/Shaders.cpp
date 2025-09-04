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