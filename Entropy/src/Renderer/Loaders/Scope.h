#pragma once
#include "TigerEngine/ClientStartup/RenderGlobals.h"
#include <d3d11.h>
#include <wrl/client.h>
#include "TigerEngine/Technique/Tfx/extern.h"

using namespace Microsoft::WRL;

class TigerScope
{
public:
	SScope Scope;

	ComPtr<ID3D11Buffer> GetPSCBuffer();
	ComPtr<ID3D11Buffer> GetVSCBuffer();

	TagHash vscb;
	TagHash pscb;
	void LoadScopeInfo(ComPtr<ID3D11Device> pDevice, ComPtr<ID3D11DeviceContext> pContext);
	void UpdateScopeBuffers(ComPtr<ID3D11DeviceContext> pContext, ExternStorage& externs);
	void Bind(ComPtr<ID3D11DeviceContext> pContext);
private:
	ComPtr<ID3D11Buffer> pscbuffer;
	ComPtr<ID3D11Buffer> vscbuffer;
	ComPtr<ID3D11Buffer> frameAuxBuffer;

	
};