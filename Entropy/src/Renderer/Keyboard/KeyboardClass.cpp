#include "KeyboardClass.h"

KeyboardClass::KeyboardClass()
{
	for (int i = 0; i < 256; i++) {
		keyStates[i] = false;
	}
}

bool KeyboardClass::KeyIsPressed(const unsigned char keycode) const
{
	return keyStates[keycode];
}

bool KeyboardClass::KeyBufferIsEmpty() const
{
	return keyBuffer.empty();
}

bool KeyboardClass::CharBufferIsEmpty() const
{
	return charBuffer.empty();
}

KeyboardEvent KeyboardClass::ReadKey()
{
	if (this->keyBuffer.empty())
	{
		return KeyboardEvent();
	}
	else
	{
		KeyboardEvent e = this->keyBuffer.front();
		this->keyBuffer.pop();
		return e;
	}
}

unsigned char KeyboardClass::ReadChar()
{
	if (this->charBuffer.empty())
	{
		return 0;
	}
	else
	{
		unsigned char e = this->charBuffer.front();
		this->charBuffer.pop();
		return e;
	}
}

void KeyboardClass::OnKeyPressed(const unsigned char key)
{
	keyStates[key] = true;
	keyBuffer.push(KeyboardEvent(KeyboardEvent::EventType::Press, key));
}

void KeyboardClass::OnKeyReleased(const unsigned char key)
{
	keyStates[key] = false;
	keyBuffer.push(KeyboardEvent(KeyboardEvent::EventType::Release, key));
}

void KeyboardClass::OnChar(const unsigned char key)
{
	charBuffer.push(key);
}

void KeyboardClass::EnableAutoRepeatKeys()
{
	autoRepeatKeys = true;
}

void KeyboardClass::DisableAutoRepeatKeys()
{
	autoRepeatKeys = false;
}

void KeyboardClass::EnableAutoRepeatChars()
{
	autoRepeatChar = true;
}

void KeyboardClass::DisableAutoRepeatChars()
{
	autoRepeatChar = false;
}

bool KeyboardClass::IsKeysAutoRepeat()
{
	return autoRepeatKeys;
}

bool KeyboardClass::IsCharsAutoRepeat()
{
	return autoRepeatChar;
}