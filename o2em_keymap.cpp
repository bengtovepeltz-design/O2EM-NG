#include "o2em_keymap.h"
#include "input_manager.h"
#include "o2em_keys.h"


SDL_Keycode O2EMKey_ToSDLKey(int o2emKey)
{
    switch (o2emKey)
    {
    case KEY_A: return SDLK_A;
    case KEY_B: return SDLK_B;
    case KEY_C: return SDLK_C;
    case KEY_D: return SDLK_D;
    case KEY_E: return SDLK_E;
    case KEY_F: return SDLK_F;
    case KEY_G: return SDLK_G;
    case KEY_H: return SDLK_H;
    case KEY_I: return SDLK_I;
    case KEY_J: return SDLK_J;
    case KEY_K: return SDLK_K;
    case KEY_L: return SDLK_L;
    case KEY_M: return SDLK_M;
    case KEY_N: return SDLK_N;
    case KEY_O: return SDLK_O;
    case KEY_P: return SDLK_P;
    case KEY_Q: return SDLK_Q;
    case KEY_R: return SDLK_R;
    case KEY_S: return SDLK_S;
    case KEY_T: return SDLK_T;
    case KEY_U: return SDLK_U;
    case KEY_V: return SDLK_V;
    case KEY_W: return SDLK_W;
    case KEY_X: return SDLK_X;
    case KEY_Y: return SDLK_Y;
    case KEY_Z: return SDLK_Z;

    case KEY_0: return SDLK_0;
    case KEY_1: return SDLK_1;
    case KEY_2: return SDLK_2;
    case KEY_3: return SDLK_3;
    case KEY_4: return SDLK_4;
    case KEY_5: return SDLK_5;
    case KEY_6: return SDLK_6;
    case KEY_7: return SDLK_7;
    case KEY_8: return SDLK_8;
    case KEY_9: return SDLK_9;

    case KEY_SPACE: return SDLK_SPACE;
    case KEY_ENTER: return SDLK_RETURN;
    case KEY_STOP: return SDLK_PERIOD;
    case KEY_SLASH: return SDLK_SLASH;

    default:
        return SDLK_UNKNOWN;
    }
}

bool O2EMKey_IsPressed(int o2emKey)
{
    SDL_Keycode sdlKey = O2EMKey_ToSDLKey(o2emKey);

    if (sdlKey == SDLK_UNKNOWN)
        return false;

    return InputManager_IsSDLKeyPressed(sdlKey);
}