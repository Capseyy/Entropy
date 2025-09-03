#include "Renderer/Engine.h"
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "DirectXTK.lib")
#undef max
#undef min


int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int)

{
	Engine engine;
	engine.Initialize(hInstance, "Entropy", "EntropyEngineWindowClass", 1920, 1080);
	while (engine.ProcessMessages() == true) 
	{
		engine.Update();
	}
    return 0;
}