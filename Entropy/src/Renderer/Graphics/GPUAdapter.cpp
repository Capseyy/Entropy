#include "GPUAdapter.h"

std::vector<GPUAdapter> GPUReader::adapters = std::vector<GPUAdapter>();

std::vector<GPUAdapter> GPUReader::GetAdapterData()
{
	if (adapters.size() > 0)
		return adapters;

	Microsoft::WRL::ComPtr<IDXGIFactory> pFactory;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(pFactory.GetAddressOf()));
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to create DXGIFactory");
		exit(-1);
	}
	IDXGIAdapter * pAdapter;
	UINT index = 0;
	while (SUCCEEDED(pFactory->EnumAdapters(index, &pAdapter)))
	{
		adapters.push_back(GPUAdapter(pAdapter));
		index++;
	}
	return adapters;
}

GPUAdapter::GPUAdapter(IDXGIAdapter* pAdapter)
{
	this->pAdapter = pAdapter;
	HRESULT hr = pAdapter->GetDesc(&adapterDesc);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to get adapter description");
	}
}