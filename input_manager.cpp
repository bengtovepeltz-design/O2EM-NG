#include "input_manager.h"
#include "input_sdl.h"
#include <cstdio>
#include <cstdlib>

static JoyState player1;
static JoyState player2;

// Keys that were already held when a game session started. They are invisible
// to the emulated Videopac keyboard until each one is physically released.
static bool g_suppressHeld[SDL_SCANCODE_COUNT] = {};
static bool g_sessionSuppression = false;

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
    // Release the launch-key suppression as soon as the physical key goes up.
    // This guarantees that a later press works even if the emulated game did
    // not query the keyboard between the release and the next press.
    if (event.type == SDL_EVENT_KEY_UP)
    {
        const SDL_Scancode scan =
            SDL_GetScancodeFromKey(event.key.key, nullptr);
        if (scan != SDL_SCANCODE_UNKNOWN)
            g_suppressHeld[scan] = false;
    }
    else if (event.type == SDL_EVENT_WINDOW_FOCUS_LOST)
    {
        // Release events are not guaranteed while unfocused.
        for (int i = 0; i < SDL_SCANCODE_COUNT; ++i)
            g_suppressHeld[i] = false;
    }

    InputSDL_HandleEvent(event);
}

void InputManager_ClearHeldState(void)
{
    InputSDL_ClearHeldState();
}

void InputManager_ResetSessionState(void)
{
    InputSDL_ResetSessionState();
}

void InputManager_BeginSessionKeySuppression(void)
{
    const bool* keyboardState = SDL_GetKeyboardState(nullptr);
    int held = 0;
    for (int i = 0; i < SDL_SCANCODE_COUNT; ++i)
    {
        g_suppressHeld[i] = keyboardState ? keyboardState[i] : false;
        if (g_suppressHeld[i])
            ++held;
    }
    g_sessionSuppression = true;
#if defined(_DEBUG)
    std::printf("O2EM-NG: session key suppression armed (%d held key(s))\n", held);
    std::fflush(stdout);
#endif
}

void InputManager_SetPlayer1State(const JoyState& state)
{
    player1 = state;
    static int lastByte = -1;
    if (InputTraceEnabled())
    {
        const int byte = JoyStateToByte(player1);
        if (byte != lastByte)
        {
            lastByte = byte;
            std::fprintf(stderr,
                "INPUT_JOY p1 byte=%02X up=%d down=%d left=%d right=%d fire=%d\n",
                byte, player1.up ? 1 : 0, player1.down ? 1 : 0,
                player1.left ? 1 : 0, player1.right ? 1 : 0,
                player1.fire ? 1 : 0);
        }
    }
}

void InputManager_SetPlayer2State(const JoyState& state)
{
    player2 = state;
    static int lastByte = -1;
    if (InputTraceEnabled())
    {
        const int byte = JoyStateToByte(player2);
        if (byte != lastByte)
        {
            lastByte = byte;
            std::fprintf(stderr,
                "INPUT_JOY p2 byte=%02X up=%d down=%d left=%d right=%d fire=%d\n",
                byte, player2.up ? 1 : 0, player2.down ? 1 : 0,
                player2.left ? 1 : 0, player2.right ? 1 : 0,
                player2.fire ? 1 : 0);
        }
    }
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

    if (scan == SDL_SCANCODE_UNKNOWN || !keyboardState)
        return false;

    if (g_sessionSuppression && g_suppressHeld[scan])
    {
        if (keyboardState[scan])
            return false; // still the key that launched the game
        g_suppressHeld[scan] = false; // released: future presses work again
    }

    return keyboardState[scan];
}