#include "Renderer/Engine.h"
#include "Renderer/Graphics/Graphics.h"


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)

{
	HRESULT hr = CoInitialize(NULL);
	if (FAILED(hr))
	{
		ErrorLogger::Log(hr, "Failed to initialize COM library.");
		return -1;
	}
	Engine engine;
	if (engine.Initialize(hInstance, "Entropy", "EntropyEngineWindowClass", 1600, 1200))
	{
		while (engine.ProcessMessages() == true)
		{
			engine.Update();
			engine.RenderFrame();
		}
	}
    return 0;
}