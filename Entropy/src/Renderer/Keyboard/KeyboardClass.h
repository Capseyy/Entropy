#pragma once
#include "Renderer/Keyboard/KeyboardEvent.h"
#include <queue>

class KeyboardClass
{
public:
	KeyboardClass();
	//~KeyboardClass();
	bool KeyIsPressed(const unsigned char keycode) const;
	bool KeyBufferIsEmpty() const;
	bool CharBufferIsEmpty() const;
	KeyboardEvent ReadKey();
	unsigned char ReadChar();
	void OnKeyPressed(const unsigned char key);
	void OnKeyReleased(const unsigned char key);
	void OnChar(const unsigned char key);
	void ClearKeyBuffer();
	void ClearCharBuffer();
	void EnableAutoRepeatKeys();
	void DisableAutoRepeatKeys();
	void EnableAutoRepeatChars();
	void DisableAutoRepeatChars();
	bool IsKeysAutoRepeat();
	bool IsCharsAutoRepeat();
private:
	bool autoRepeatKeys = false;
	bool autoRepeatChar = false;
	bool keyStates[256];
	std::queue<KeyboardEvent> keyBuffer;
	std::queue<unsigned char> charBuffer;
};

