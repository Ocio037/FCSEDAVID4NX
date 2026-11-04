#define NOMINMAX
#include"TitiInput.h"
#ifdef _WIN32
#include<Windows.h>
#endif

Player::TitiInputState Player::TitiInput::ReadKeyboard()
{
	TitiInputState input;
#ifdef _WIN32
	if (GetAsyncKeyState('A') & 0x8000) input.Movement.x -= 1.0f;
	if (GetAsyncKeyState('D') & 0x8000) input.Movement.x += 1.0f;
	if (GetAsyncKeyState('W') & 0x8000) input.Movement.z += 1.0f;
	if (GetAsyncKeyState('S') & 0x8000) input.Movement.z -= 1.0f;
	input.JumpDown = (GetAsyncKeyState(VK_SPACE) & 0x8000) != 0;
#endif
	return input;
}
