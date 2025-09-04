#pragma once
#include "Renderer/Tools/ErrorLogger.h"
#include <d3d11.h>
#include <wrl/client.h>
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "DirectXTK.lib")
#pragma comment(lib, "DXGI.lib")
#include <vector>


class GPUAdapter
{
public:
	GPUAdapter(IDXGIAdapter* pAdapter);
	IDXGIAdapter* pAdapter = nullptr;
	DXGI_ADAPTER_DESC adapterDesc;
};

class GPUReader
{
public:
	static std::vector<GPUAdapter> GetAdapterData();
private:
	static std::vector<GPUAdapter> adapters;

};

