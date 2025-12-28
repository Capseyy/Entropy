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
				this->gfx.camera.AdjustRotation((float)me.GetPosY() / 8 * 0.01f, 0.0f, (float)me.GetPosX() / 8 * -0.01f);
			}
		}
	}
	float cameraSpeed;
	if (keyboard.KeyIsPressed(VK_SHIFT))
	{
		if (keyboard.KeyIsPressed(VK_SPACE)) {
			cameraSpeed = gfx.camera.GetSpeed() * 100;
		}
		else {
			cameraSpeed = gfx.camera.GetSpeed() * 10;
		}
		
	}
	else
	{
		cameraSpeed = gfx.camera.GetSpeed();
	}

	if (keyboard.KeyIsPressed('S'))
	{
		DirectX::XMVECTOR forward = gfx.camera.GetForwardVector();    
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(forward, cameraSpeed * dt);
		this->gfx.camera.AdjustPosition(delta); 
	}

	if (keyboard.KeyIsPressed('W'))
	{
		DirectX::XMVECTOR backward = gfx.camera.GetBackwardVector();   
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(backward, cameraSpeed * dt);
		this->gfx.camera.AdjustPosition(delta);              
	}

	if (keyboard.KeyIsPressed('A'))
	{
		DirectX::XMVECTOR left = gfx.camera.GetLeftVector();    
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(left, cameraSpeed * dt); 
		this->gfx.camera.AdjustPosition(delta);              
	}

	if (keyboard.KeyIsPressed('D'))
	{
		DirectX::XMVECTOR right = gfx.camera.GetRightVector();    
		DirectX::XMVECTOR delta = DirectX::XMVectorScale(right, cameraSpeed * dt);
		this->gfx.camera.AdjustPosition(delta);  
	}

}

void Engine::RenderFrame()
{
	gfx.RenderFrame();
}