/*
 * O2EM-NG modification notice (Clarified Artistic License, section 3):
 * This file replaces the original O2EM delay helper with an SDL3
 * implementation for O2EM-NG. O2EM-NG modifications: Copyright (c) 2026
 * Bengt-Ove Peltz. See Docs/COPYRIGHTS.txt.
 */

#include "timing_sdl.h"
#include <SDL3/SDL.h>

void O2EM_Delay(int milliseconds)
{
    if (milliseconds < 0)
        milliseconds = 0;

    SDL_Delay(static_cast<Uint32>(milliseconds));
}