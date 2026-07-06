#pragma once

#include <SDL3/SDL.h>
#include <cstdint>

using Byte = uint8_t;

struct JoyState
{
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool fire = false;
};

void InputManager_HandleEvent(const SDL_Event& event);

void InputManager_SetPlayer1State(const JoyState& state);
void InputManager_SetPlayer2State(const JoyState& state);

JoyState InputManager_GetPlayer1State();
JoyState InputManager_GetPlayer2State();

Byte InputManager_GetJoystickByte(int player);

bool InputManager_IsSDLKeyPressed(SDL_Keycode key);