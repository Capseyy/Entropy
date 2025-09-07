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
	float dt = timer.GetMilisecondsElapsed();
	timer.Restart();
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
		if (mouse.IsRightDown())
		{
			if (me.GetType() == MouseEvent::EventType::RAW_MOVE)
			{
				this->gfx.camera.AdjustRotation((float)me.GetPosY()/8 * 0.01f, (float)me.GetPosX()/10 * 0.01f, 0);
			}
		}
	}
	float cameraSpeed;
	if (keyboard.KeyIsPressed(VK_SHIFT))
	{
		cameraSpeed = gfx.camera.GetSpeed() * 2;
	}
	else
	{
		cameraSpeed = gfx.camera.GetSpeed();
	}

	if (keyboard.KeyIsPressed('W'))
	{
		DirectX::XMVECTOR forward = gfx.camera.GetForwardVector();     // XMVECTOR
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(forward, cameraSpeed * dt); // scale by scalar
		this->gfx.camera.AdjustPosition(delta);               // whatever type it expects
	}

	if (keyboard.KeyIsPressed('S'))
	{
		DirectX::XMVECTOR backward = gfx.camera.GetBackwardVector();     // XMVECTOR
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(backward, cameraSpeed * dt); // scale by scalar
		this->gfx.camera.AdjustPosition(delta);               // whatever type it expects
	}

	if (keyboard.KeyIsPressed('A'))
	{
		DirectX::XMVECTOR left = gfx.camera.GetLeftVector();     // XMVECTOR
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(left, cameraSpeed * dt); // scale by scalar
		this->gfx.camera.AdjustPosition(delta);               // whatever type it expects
	}

	if (keyboard.KeyIsPressed('D'))
	{
		DirectX::XMVECTOR right = gfx.camera.GetRightVector();     // XMVECTOR
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(right, cameraSpeed * dt); // scale by scalar
		this->gfx.camera.AdjustPosition(delta);               // whatever type it expects
	}
	
}

void Engine::RenderFrame()
{
	gfx.RenderFrame();
}