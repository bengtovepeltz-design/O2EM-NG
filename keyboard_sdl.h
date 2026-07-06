#pragma once

#include <cstdint>

using Byte = uint8_t;

void Set_Old_Int9(void);
void init_keyboard(void);
void handle_key(void);
void set_joykeys(int joy, int up, int down, int left, int right, int fire);
void set_systemkeys(int k_quit, int k_pause, int k_debug, int k_reset,
    int k_screencap, int k_save, int k_load, int k_inject);
void set_defjoykeys(int joy, int sc);
void set_defsystemkeys(void);
Byte keyjoy(int jn);