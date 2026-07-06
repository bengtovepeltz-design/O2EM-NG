#include "timefunc_sdl.h"
#include <SDL3/SDL.h>

long gettimeticks(void)
{
    return static_cast<long>(SDL_GetTicks() * 10);
}