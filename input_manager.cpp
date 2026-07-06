#include "input_manager.h"
#include "input_sdl.h"

static JoyState player1;
static JoyState player2;

static Byte JoyStateToByte(const JoyState& joy)
{
    Byte d = 0xFF;

    if (joy.up)    d &= 0xFE;
    if (joy.down)  d &= 0xFB;
    if (joy.left)  d &= 0xF7;
    if (joy.right) d &= 0xFD;
    if (joy.fire)  d &= 0xEF;

    return d;
}

void InputManager_HandleEvent(const SDL_Event& event)
{
    InputSDL_HandleEvent(event);
}

void InputManager_SetPlayer1State(const JoyState& state)
{
    player1 = state;
}

void InputManager_SetPlayer2State(const JoyState& state)
{
    player2 = state;
}

JoyState InputManager_GetPlayer1State()
{
    return player1;
}

JoyState InputManager_GetPlayer2State()
{
    return player2;
}

Byte InputManager_GetJoystickByte(int player)
{
    if (player == 0)
        return JoyStateToByte(player1);

    if (player == 1)
        return JoyStateToByte(player2);

    return 0xFF;
}
bool InputManager_IsSDLKeyPressed(SDL_Keycode key)
{
    const bool* keyboardState = SDL_GetKeyboardState(nullptr);
    SDL_Scancode scan = SDL_GetScancodeFromKey(key, nullptr);

    if (scan == SDL_SCANCODE_UNKNOWN)
        return false;

    return keyboardState[scan];
}