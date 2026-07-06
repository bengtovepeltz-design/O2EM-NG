#include "keyboard_sdl.h"
#include "input_manager.h"

Byte new_int = 0;
int NeedsPoll = 0;
Byte key_done = 0;
Byte key_debug = 0;

int joykeys[2][5] = {
    {0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0}
};

int joykeystab[128] = { 0 };
int syskeys[8] = { 0 };

void Set_Old_Int9(void)
{
}

void init_keyboard(void)
{
    key_done = 0;
    key_debug = 0;
    new_int = 1;
    NeedsPoll = 0;
}

void handle_key(void)
{
}

void set_joykeys(int joy, int up, int down, int left, int right, int fire)
{
}

void set_systemkeys(int k_quit, int k_pause, int k_debug, int k_reset,
    int k_screencap, int k_save, int k_load, int k_inject)
{
}

void set_defjoykeys(int joy, int sc)
{
}

void set_defsystemkeys(void)
{
}

Byte keyjoy(int jn)
{
    return InputManager_GetJoystickByte(jn);
}