#include "input_sdl.h"
#include "input_manager.h"
#include "vdc_stub.h"
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
#include <windows.h>
#include <xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

static bool InputTraceEnabled()
{
    static int enabled = -1;
    if (enabled < 0)
    {
        const char* env = std::getenv("O2EM_INPUT_TRACE");
        enabled = (env && *env && *env != '0') ? 1 : 0;
    }
    return enabled == 1;
}

static JoyState keyboardP1;
static JoyState keyboardP2;

static JoyState buttonP1;
static JoyState buttonP2;

static JoyState axisP1;
static JoyState axisP2;

static SDL_Gamepad* gamepads[2] = { nullptr, nullptr };
static SDL_JoystickID gamepadIds[2] = { 0, 0 };

static const Sint16 AXIS_DEADZONE = 12000;

static bool controllerPortsSwapped = false;

#if defined(_WIN32)
static bool slotIsXbox[2] = { false, false };
static int slotXInputUser[2] = { -1, -1 };

static bool XInputDisabled()
{
    static int disabled = -1;
    if (disabled < 0)
    {
        const char* env = std::getenv("O2EM_INPUT_DISABLE_XINPUT");
        disabled = (env && *env && *env != '0') ? 1 : 0;
    }
    return disabled == 1;
}

static bool XInputConnected(int user)
{
    XINPUT_STATE state{};
    return XInputGetState(static_cast<DWORD>(user), &state) == ERROR_SUCCESS;
}

static int NextFreeXInputUser()
{
    static int skipUsers = -1;
    if (skipUsers < 0)
    {
        const char* env = std::getenv("O2EM_INPUT_XINPUT_SKIP_USERS");
        skipUsers = (env && *env) ? std::atoi(env) : 0;
    }
    for (int user = skipUsers; user < 4; ++user)
    {
        if (!XInputConnected(user))
            continue;
        bool used = false;
        for (int slot = 0; slot < 2; ++slot)
            if (slotXInputUser[slot] == user)
                used = true;
        if (!used)
            return user;
    }
    return -1;
}
#endif

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
    JoyState gamepadP1 = CombineState(JoyState{}, buttonP1, axisP1);
    JoyState gamepadP2 = CombineState(JoyState{}, buttonP2, axisP2);

    if (controllerPortsSwapped)
    {
        InputManager_SetPlayer1State(CombineState(keyboardP1, gamepadP2, JoyState{}));
        InputManager_SetPlayer2State(CombineState(keyboardP2, gamepadP1, JoyState{}));
    }
    else
    {
        InputManager_SetPlayer1State(CombineState(keyboardP1, gamepadP1, JoyState{}));
        InputManager_SetPlayer2State(CombineState(keyboardP2, gamepadP2, JoyState{}));
    }
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
#if defined(_WIN32)
                slotIsXbox[i] = false;
                slotXInputUser[i] = -1;
                const SDL_GamepadType padType = SDL_GetGamepadType(gamepads[i]);
                if (padType == SDL_GAMEPAD_TYPE_XBOX360 ||
                    padType == SDL_GAMEPAD_TYPE_XBOXONE)
                {
                    slotIsXbox[i] = true;
                    if (!XInputDisabled())
                        slotXInputUser[i] = NextFreeXInputUser();
                }
#endif
                if (InputTraceEnabled())
                {
                    char* mapping = SDL_GetGamepadMapping(gamepads[i]);
                    std::fprintf(stderr,
                        "INPUT_GAMEPAD slot=%d id=%u name=%s type=%d vendor=%04X product=%04X mapping=%s\n",
                        i + 1, static_cast<unsigned>(id),
                        SDL_GetGamepadName(gamepads[i]) ? SDL_GetGamepadName(gamepads[i]) : "?",
                        static_cast<int>(SDL_GetGamepadType(gamepads[i])),
                        static_cast<unsigned>(SDL_GetGamepadVendor(gamepads[i])),
                        static_cast<unsigned>(SDL_GetGamepadProduct(gamepads[i])),
                        mapping ? mapping : "(none)");
                    SDL_free(mapping);
                }
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
#if defined(_WIN32)
            slotIsXbox[i] = false;
            slotXInputUser[i] = -1;
#endif

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

    if (InputTraceEnabled())
    {
        std::fprintf(stderr, "INPUT_GAMEPAD_BUTTON slot=%d button=%d pressed=%d\n",
            slot, static_cast<int>(event.gbutton.button), pressed ? 1 : 0);
    }

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

    case SDL_GAMEPAD_BUTTON_NORTH:
        if (pressed)
        {
            controllerPortsSwapped = !controllerPortsSwapped;
            const char* message = controllerPortsSwapped
                ? "CONTROLLER PORTS SWAPPED"
                : "CONTROLLER PORTS NORMAL";

            SDL_Log("%s", message);
            VDCStub_ShowMessage(message);
        }
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

    if (InputTraceEnabled())
    {
        static int lastState[2] = {-1, -1};
        const int packed = (joy->up ? 1 : 0) | (joy->down ? 2 : 0) |
                           (joy->left ? 4 : 0) | (joy->right ? 8 : 0);
        if (packed != lastState[slot])
        {
            lastState[slot] = packed;
            std::fprintf(stderr,
                "INPUT_GAMEPAD_AXIS slot=%d axis=%d value=%d -> up=%d down=%d left=%d right=%d\n",
                slot, static_cast<int>(event.gaxis.axis),
                static_cast<int>(event.gaxis.value),
                joy->up ? 1 : 0, joy->down ? 1 : 0,
                joy->left ? 1 : 0, joy->right ? 1 : 0);
        }
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

static void ClearHeldStateInternal()
{
    // Keyboard fallback and gamepad buttons/axes are all transient. Clearing
    // them prevents a lost key/button release (focus loss, window switch,
    // game exit) from leaving emulation logically pressed.
    keyboardP1 = JoyState{};
    keyboardP2 = JoyState{};
    buttonP1 = JoyState{};
    buttonP2 = JoyState{};
    axisP1 = JoyState{};
    axisP2 = JoyState{};
}

void InputSDL_ClearHeldState(void)
{
    ClearHeldStateInternal();
    PushStates();
}

void InputSDL_ResetSessionState(void)
{
    ClearHeldStateInternal();

    // Controller port swapping is a session toggle, not a persisted user
    // setting (it is not stored in o2em-ng.cfg), so a new game session starts
    // from the normal assignment.
    controllerPortsSwapped = false;

    PushStates();
}


void InputSDL_EnsureGamepadsOpen(void)
{
    int skip = 0;
    if (const char* env = std::getenv("O2EM_INPUT_SKIP_GAMEPADS"))
        skip = std::atoi(env);
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    if (ids)
    {
        for (int i = 0; i < count; ++i)
        {
            if (i < skip)
            {
                if (InputTraceEnabled())
                    std::fprintf(stderr, "INPUT_GAMEPAD_SKIP index=%d id=%u\n",
                                 i, static_cast<unsigned>(ids[i]));
                continue;
            }
            bool already = false;
            for (int slot = 0; slot < 2; ++slot)
                if (gamepads[slot] && gamepadIds[slot] == ids[i])
                    already = true;
            if (!already)
                OpenGamepad(ids[i]);
        }
        SDL_free(ids);
    }
    PushStates();
}

static void RefreshSlot(int slot)
{
    SDL_Gamepad* pad = gamepads[slot];
    if (!pad)
        return;

    JoyState& buttons = slot == 0 ? buttonP1 : buttonP2;
    JoyState& axes = slot == 0 ? axisP1 : axisP2;

#if defined(_WIN32)
    // SDL3's event path can open an XInput gamepad and still fail to deliver
    // its reports on some Windows/XInput combinations. The same controller is
    // proven through XInput (MAME's XInput provider). Use XInput as the
    // backend for XInput-class gamepads, preserving this layer's existing
    // JoyState and slot semantics.
    if (slotIsXbox[slot] && !XInputDisabled())
    {
        if (slotXInputUser[slot] < 0)
            slotXInputUser[slot] = NextFreeXInputUser();
        if (slotXInputUser[slot] >= 0)
        {
            XINPUT_STATE state{};
            if (XInputGetState(static_cast<DWORD>(slotXInputUser[slot]), &state) ==
                ERROR_SUCCESS)
            {
                const XINPUT_GAMEPAD& g = state.Gamepad;
                buttons.up = (g.wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
                buttons.down = (g.wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
                buttons.left = (g.wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
                buttons.right = (g.wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
                buttons.fire = (g.wButtons & XINPUT_GAMEPAD_A) != 0;
                axes.left = g.sThumbLX < -AXIS_DEADZONE;
                axes.right = g.sThumbLX > AXIS_DEADZONE;
                axes.up = g.sThumbLY > AXIS_DEADZONE;
                axes.down = g.sThumbLY < -AXIS_DEADZONE;
                if (InputTraceEnabled())
                {
                    static int lastState[2] = {-1, -1};
                    const int packed =
                        static_cast<int>(g.wButtons) |
                        (static_cast<int>(axes.up) << 16) |
                        (static_cast<int>(axes.down) << 17) |
                        (static_cast<int>(axes.left) << 18) |
                        (static_cast<int>(axes.right) << 19) |
                        (static_cast<int>(buttons.fire) << 20);
                    if (packed != lastState[slot])
                    {
                        lastState[slot] = packed;
                        std::fprintf(stderr,
                            "INPUT_XINPUT slot=%d user=%d buttons=%04X axis=(%d,%d) state=%d%d%d%d%d\n",
                            slot + 1, slotXInputUser[slot],
                            static_cast<unsigned>(g.wButtons),
                            static_cast<int>(g.sThumbLX),
                            static_cast<int>(g.sThumbLY),
                            axes.up ? 1 : 0, axes.down ? 1 : 0,
                            axes.left ? 1 : 0, axes.right ? 1 : 0,
                            buttons.fire ? 1 : 0);
                    }
                }
                return;
            }
            slotXInputUser[slot] = -1;
        }
    }
#endif

    buttons.up = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_UP);
    buttons.down = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
    buttons.left = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);
    buttons.right = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
    buttons.fire = SDL_GetGamepadButton(pad, SDL_GAMEPAD_BUTTON_SOUTH);

    const Sint16 x = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTX);
    const Sint16 y = SDL_GetGamepadAxis(pad, SDL_GAMEPAD_AXIS_LEFTY);
    axes.left = x < -AXIS_DEADZONE;
    axes.right = x > AXIS_DEADZONE;
    axes.up = y < -AXIS_DEADZONE;
    axes.down = y > AXIS_DEADZONE;

    if (InputTraceEnabled())
    {
        static int lastBits[2] = {-1, -1};
        int bits = (buttons.up ? 1 : 0) | (buttons.down ? 2 : 0) |
                   (buttons.left ? 4 : 0) | (buttons.right ? 8 : 0) |
                   (buttons.fire ? 16 : 0) | (axes.up ? 32 : 0) |
                   (axes.down ? 64 : 0) | (axes.left ? 128 : 0);
        int bits2 = (axes.right ? 1 : 0);
        int combined = bits | (bits2 << 8);
        if (combined != lastBits[slot])
        {
            lastBits[slot] = combined;
            std::fprintf(stderr,
                "INPUT_REFRESH slot=%d dpad=%d%d%d%d fire=%d axis=(%d,%d) state=%d%d%d%d%d\n",
                slot + 1, buttons.up ? 1 : 0, buttons.down ? 1 : 0,
                buttons.left ? 1 : 0, buttons.right ? 1 : 0,
                buttons.fire ? 1 : 0, static_cast<int>(x), static_cast<int>(y),
                axes.up ? 1 : 0, axes.down ? 1 : 0, axes.left ? 1 : 0,
                axes.right ? 1 : 0, buttons.fire ? 1 : 0);
        }
    }
}

void InputSDL_RefreshGamepadStates(void)
{
    RefreshSlot(0);
    RefreshSlot(1);
    PushStates();
}

void InputSDL_HandleEvent(const SDL_Event& event)
{
    switch (event.type)
    {
    case SDL_EVENT_GAMEPAD_ADDED:
        if (InputTraceEnabled())
            std::fprintf(stderr, "INPUT_GAMEPAD_ADDED id=%u\n", static_cast<unsigned>(event.gdevice.which));
        OpenGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_GAMEPAD_REMOVED:
        if (InputTraceEnabled())
            std::fprintf(stderr, "INPUT_GAMEPAD_REMOVED id=%u\n", static_cast<unsigned>(event.gdevice.which));
        CloseGamepad(event.gdevice.which);
        break;

    case SDL_EVENT_WINDOW_FOCUS_LOST:
        // No release events arrive while unfocused; drop held state so a
        // button held during the switch cannot stick into the next session.
        ClearHeldStateInternal();
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
