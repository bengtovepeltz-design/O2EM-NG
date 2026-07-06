#include "input_sdl.h"
#include "input_manager.h"
#include <SDL3/SDL.h>

static JoyState keyboardP1;
static JoyState keyboardP2;

static JoyState buttonP1;
static JoyState buttonP2;

static JoyState axisP1;
static JoyState axisP2;

static SDL_Gamepad* gamepads[2] = { nullptr, nullptr };
static SDL_JoystickID gamepadIds[2] = { 0, 0 };

static const Sint16 AXIS_DEADZONE = 12000;

static JoyState CombineState(const JoyState& keyboard, const JoyState& buttons, const JoyState& axes)
{
    JoyState result{};

    result.up    = keyboard.up    || buttons.up    || axes.up;
    result.down  = keyboard.down  || buttons.down  || axes.down;
    result.left  = keyboard.left  || buttons.left  || axes.left;
    result.right = keyboard.right || buttons.right || axes.right;
    result.fire  = keyboard.fire  || buttons.fire  || axes.fire;

    return result;
}

static void PushStates()
{
    InputManager_SetPlayer1State(CombineState(keyboardP1, buttonP1, axisP1));
    InputManager_SetPlayer2State(CombineState(keyboardP2, buttonP2, axisP2));
}

static void OpenGamepad(SDL_JoystickID id)
{
    for (int i = 0; i < 2; i++)
    {
        if (gamepadIds[i] == id)
            return;
    }

    for (int i = 0; i < 2; i++)
    {
        if (!gamepads[i])
        {
            gamepads[i] = SDL_OpenGamepad(id);

            if (gamepads[i])
            {
                gamepadIds[i] = id;
                SDL_Log("Gamepad %d connected: %s", i + 1, SDL_GetGamepadName(gamepads[i]));
            }
            else
            {
                SDL_Log("Failed to open gamepad: %s", SDL_GetError());
            }

            return;
        }
    }
}

static void CloseGamepad(SDL_JoystickID id)
{
    for (int i = 0; i < 2; i++)
    {
        if (gamepadIds[i] == id)
        {
            SDL_Log("Gamepad %d disconnected", i + 1);

            SDL_CloseGamepad(gamepads[i]);
            gamepads[i] = nullptr;
            gamepadIds[i] = 0;

            if (i == 0)
            {
                buttonP1 = JoyState{};
                axisP1 = JoyState{};
            }
            else
            {
                buttonP2 = JoyState{};
                axisP2 = JoyState{};
            }

            PushStates();
            return;
        }
    }
}

static int GetSlotFromGamepad(SDL_JoystickID id)
{
    if (gamepadIds[0] == id)
        return 0;

    if (gamepadIds[1] == id)
        return 1;

    return -1;
}

static JoyState* GetButtonStateFromSlot(int slot)
{
    if (slot == 0)
        return &buttonP1;

    if (slot == 1)
        return &buttonP2;

    return nullptr;
}

static JoyState* GetAxisStateFromSlot(int slot)
{
    if (slot == 0)
        return &axisP1;

    if (slot == 1)
        return &axisP2;

    return nullptr;
}

static void HandleGamepadButton(const SDL_Event& event, bool pressed)
{
    int slot = GetSlotFromGamepad(event.gbutton.which);
    JoyState* joy = GetButtonStateFromSlot(slot);

    if (!joy)
        return;

    switch (event.gbutton.button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        joy->up = pressed;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        joy->down = pressed;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        joy->left = pressed;
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        joy->right = pressed;
        break;

    case SDL_GAMEPAD_BUTTON_SOUTH:
        joy->fire = pressed;
        break;

    default:
        break;
    }
}

static void HandleGamepadAxis(const SDL_Event& event)
{
    int slot = GetSlotFromGamepad(event.gaxis.which);
    JoyState* joy = GetAxisStateFromSlot(slot);

    if (!joy)
        return;

    if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTX)
    {
        joy->left  = (event.gaxis.value < -AXIS_DEADZONE);
        joy->right = (event.gaxis.value >  AXIS_DEADZONE);
    }

    if (event.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
    {
        joy->up   = (event.gaxis.value < -AXIS_DEADZONE);
        joy->down = (event.gaxis.value >  AXIS_DEADZONE);
    }
}

static void HandleKeyboard(const SDL_Event& event, bool pressed)
{
    SDL_Keycode key = event.key.key;

    // Player 1 keyboard fallback: Arrow keys + Right Shift
    if (key == SDLK_UP)       keyboardP1.up = pressed;
    if (key == SDLK_DOWN)     keyboardP1.down = pressed;
    if (key == SDLK_LEFT)     keyboardP1.left = pressed;
    if (key == SDLK_RIGHT)    keyboardP1.right = pressed;
    if (key == SDLK_RSHIFT)   keyboardP1.fire = pressed;

    // Player 1 alternative: Numpad 8/5/4/6 + Right Ctrl
    if (key == SDLK_KP_8)     keyboardP1.up = pressed;
    if (key == SDLK_KP_5)     keyboardP1.down = pressed;
    if (key == SDLK_KP_4)     keyboardP1.left = pressed;
    if (key == SDLK_KP_6)     keyboardP1.right = pressed;
    if (key == SDLK_RCTRL)    keyboardP1.fire = pressed;

    // Player 2 keyboard fallback: WASD + Space
    if (key == SDLK_W)        keyboardP2.up = pressed;
    if (key == SDLK_S)        keyboardP2.down = pressed;
    if (key == SDLK_A)        keyboardP2.left = pressed;
    if (key == SDLK_D)        keyboardP2.right = pressed;
    if (key == SDLK_SPACE)    keyboardP2.fire = pressed;
}

void InputSDL_HandleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
        OpenGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        CloseGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        HandleGamepadButton(event, true);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        HandleGamepadButton(event, false);
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        HandleGamepadAxis(event);
        break;

    case SDL_EVENT_KEY_DOWN:
        HandleKeyboard(event, true);
        break;

    case SDL_EVENT_KEY_UP:
        HandleKeyboard(event, false);
        break;

    default:
        return;
    }

    PushStates();
}
