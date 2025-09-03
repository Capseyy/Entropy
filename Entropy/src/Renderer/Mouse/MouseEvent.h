#pragma once

struct MousePoint
{
	int x;
	int y;
};

class MouseEvent
{
public:
	enum EventType
	{
		Move,
		LPress,
		LRelease,
		RPress,
		RRelease,
		MPress,
		MRelease,
		WheelUp,
		WheelDown,
		Invalid,
		RAW_MOVE
	};

private:
	EventType type;
	int x;
	int y;

public:
	MouseEvent();
	MouseEvent(const EventType type, const MousePoint& pos);
	bool IsValid() const;
	EventType GetType() const;
	MousePoint GetPos() const;
	int GetPosX() const;
	int GetPosY() const;
};

