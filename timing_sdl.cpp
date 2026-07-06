#include "timing_sdl.h"
#include <SDL3/SDL.h>

void O2EM_Delay(int milliseconds)
{
    if (milliseconds < 0)
        milliseconds = 0;

    SDL_Delay(static_cast<Uint32>(milliseconds));
}