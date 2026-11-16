/*
 * O2EM-NG modification notice (Clarified Artistic License, section 3):
 * This file replaces the original O2EM timing helper with an SDL3
 * implementation for O2EM-NG. O2EM-NG modifications: Copyright (c) 2026
 * Bengt-Ove Peltz. See Docs/COPYRIGHTS.txt.
 */

#include "timefunc_sdl.h"
#include <SDL3/SDL.h>

long gettimeticks(void)
{
    return static_cast<long>(SDL_GetTicks() * 10);
}