#include "MouseClass.h"

void MouseClass::OnLeftPressed(int x, int y)
{
	leftIsDown = true;
	MouseEvent me(MouseEvent::EventType::LPress, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnLeftReleased(int x, int y)
{
	leftIsDown = false;
	MouseEvent me(MouseEvent::EventType::LRelease, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnRightPressed(int x, int y)
{
	rightIsDown = true;
	MouseEvent me(MouseEvent::EventType::RPress, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnRightReleased(int x, int y)
{
	rightIsDown = false;
	MouseEvent me(MouseEvent::EventType::RRelease, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnMiddlePressed(int x, int y)
{
	middleIsDown = true;
	MouseEvent me(MouseEvent::EventType::MPress, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnMiddleReleased(int x, int y)
{
	middleIsDown = false;
	MouseEvent me(MouseEvent::EventType::MRelease, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnWheelUp(int x, int y)
{
	MouseEvent me(MouseEvent::EventType::WheelUp, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnWheelDown(int x, int y)
{
	MouseEvent me(MouseEvent::EventType::WheelDown, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnMouseMove(int x, int y)
{
	this->x = x;
	this->y = y;
	MouseEvent me(MouseEvent::EventType::Move, { x, y });
	eventBuffer.push(me);
}

void MouseClass::OnMouseMoveRaw(int x, int y)
{
	MouseEvent me(MouseEvent::EventType::RAW_MOVE, { x, y });
	eventBuffer.push(me);
}

bool MouseClass::IsLeftDown()
{
	return leftIsDown;
}
bool MouseClass::IsRightDown()
{
	return rightIsDown;
}
bool MouseClass::IsMiddleDown()
{
	return middleIsDown;
}
int MouseClass::GetPosX()
{
	return x;
}
int MouseClass::GetPosY()
{
	return y;
}
MousePoint MouseClass::GetPos()
{
	return { x, y };
}
bool MouseClass::EventBufferIsEmpty()
{
	return eventBuffer.empty();
}
MouseEvent MouseClass::ReadEvent()
{
	if (eventBuffer.empty())
	{
		return MouseEvent();
	}
	else
	{
		MouseEvent e = eventBuffer.front();
		eventBuffer.pop();
		return e;
	}
}

