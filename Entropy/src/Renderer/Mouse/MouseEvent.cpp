#include "MouseEvent.h"

MouseEvent::MouseEvent()
	:
	type(EventType::Invalid),
	x(0),
	y(0)
{}

MouseEvent::MouseEvent(const EventType type, const MousePoint& pos)
	:
	type(type),
	x(pos.x),
	y(pos.y)
{}

bool MouseEvent::IsValid() const
{
	return type != EventType::Invalid;
}

MouseEvent::EventType MouseEvent::GetType() const
{
	return type;
}

MousePoint MouseEvent::GetPos() const
{
	return { x, y };
}

int MouseEvent::GetPosX() const
{
	return x;
}

int MouseEvent::GetPosY() const
{
	return y;
}

