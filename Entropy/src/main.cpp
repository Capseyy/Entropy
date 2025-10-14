#include "Renderer/Engine.h"
#include "Renderer/Graphics/Graphics.h"
#include "TigerEngine/package.h"
#include "TigerEngine/globaldata.h"
#include "TigerEngine/String/string.h"
#include <windows.h>
#include <cstdio>

void EnsureConsole()
{
	AllocConsole();
	FILE* fp;
	freopen_s(&fp, "CONOUT$", "w", stdout);
	freopen_s(&fp, "CONOUT$", "w", stderr);
	freopen_s(&fp, "CONIN$", "r", stdin);
}

int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)

{
	EnsureConsole();
	auto RedactedKeys = Read_Redacted_Keys();
	auto pcache = GeneratePackageCache(RedactedKeys);
	for (const auto& pkg : pcache) {
		GlobalData::getMap().insert({ pkg.first, pkg.second });
		for (const auto& h64entry : pkg.second.h64s) {
			GlobalData::getH64().insert({ h64entry.hash64, TagHash(h64entry.hash32, true)});
		}
	}
	printf("Loaded %zu packages\n", GlobalData::getMap().size());
	printf("Loaded %zu h64 entries\n", GlobalData::getH64().size());
	auto start = std::chrono::high_resolution_clock::now();
	auto StringMap = GenerateStringMap();
	auto end = std::chrono::high_resolution_clock::now();
	std::chrono::duration<double> elapsed = end - start;
	printf("Loaded String Map with %zu entries in %.3f seconds\n",
		StringMap.size(), elapsed.count());
	//SearchBungieFiles(0x3D2BDBAC);
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to initialize COM library.");
		return -1;
	}

	Engine engine;
	if (engine.Initialize(hInstance, "Entropy", "EntropyEngineWindowClass", 1920, 1080))
	{
		while (engine.ProcessMessages() == true)
		{
			engine.Update();
			engine.RenderFrame();
		}
	}
    return 0;
}