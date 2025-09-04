#include "Engine.h"

bool Engine::Initialize(HINSTANCE hInstance, std::string window_title, std::string window_class, int width, int height) 
{
	if (!render_window.Initialize(this, hInstance, window_title, window_class, width, height)) {
		return false;
	};

	if (!gfx.Initialize(render_window.GetHWND(), width, height)) {
		return false;
	}
	return true;
		

}

bool Engine::ProcessMessages() {
	return render_window.ProcessMessages();
}	

void Engine::Update()
{
	while (!keyboard.CharBufferIsEmpty())
	{
		unsigned char ch = keyboard.ReadChar();
	}

	while (!keyboard.KeyBufferIsEmpty())
	{
		KeyboardEvent kbe = keyboard.ReadKey();
		unsigned char keycode = kbe.GetKeyCode();
	}

	while (!mouse.EventBufferIsEmpty())
	{
		MouseEvent me = mouse.ReadEvent();
	}
}

void Engine::RenderFrame()
{
	gfx.RenderFrame();
}