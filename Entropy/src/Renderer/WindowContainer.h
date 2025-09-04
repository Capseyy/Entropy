#pragma once
#include "Renderer/RenderWindow.h"
#include "Renderer/Keyboard/KeyboardClass.h"
#include "Renderer/Mouse/MouseClass.h"
#include <memory>
#include "Renderer/Graphics/Graphics.h"

class WindowContainer
{
public:
	WindowContainer();
	LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
protected:
	RenderWindow render_window;
	KeyboardClass keyboard;
	MouseClass mouse;
	Graphics gfx;
};

