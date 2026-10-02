#include "o2em_keyboard_bridge.h"
#include "o2em_keymap.h"
#include "src/mcs48/legacy_observation.h"

bool O2EMKeyboard_IsKeyPressed(int o2emKeyCode)
{
    if (mcs48::observation::input) return mcs48::observation::input(o2emKeyCode);
    return O2EMKey_IsPressed(o2emKeyCode);
}