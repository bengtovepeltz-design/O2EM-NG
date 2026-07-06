#include "o2em_keyboard_bridge.h"
#include "o2em_keymap.h"

bool O2EMKeyboard_IsKeyPressed(int o2emKeyCode)
{
    return O2EMKey_IsPressed(o2emKeyCode);
}