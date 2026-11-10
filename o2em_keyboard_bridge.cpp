#include "o2em_keyboard_bridge.h"
#include "o2em_keymap.h"
#include "src/mcs48/legacy_observation.h"
#include "vmachine.h"
#include <cstdio>
#include <cstdlib>

bool O2EMKeyboard_IsKeyPressed(int o2emKeyCode)
{
    if (mcs48::observation::input) return mcs48::observation::input(o2emKeyCode);

    const bool pressed = O2EMKey_IsPressed(o2emKeyCode);

    static int traceEnabled = -1;
    static bool lastPressed[64] = {};
    if (traceEnabled < 0)
    {
        const char* env = std::getenv("O2EM_INPUT_TRACE");
        traceEnabled = (env && *env && *env != '0') ? 1 : 0;
    }
    if (traceEnabled && o2emKeyCode >= 0 && o2emKeyCode < 64)
    {
        if (pressed != lastPressed[o2emKeyCode])
        {
            lastPressed[o2emKeyCode] = pressed;
            std::fprintf(stderr,
                "INPUT_KEY frame=%d clk=%d key=%d pressed=%d\n",
                frame, master_clk, o2emKeyCode, pressed ? 1 : 0);
        }
    }

    return pressed;
}