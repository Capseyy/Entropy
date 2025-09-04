#include "Renderer/Engine.h"
#include "Renderer/Graphics/Graphics.h"


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)

{
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