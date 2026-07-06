#pragma once

#include <SDL3/SDL.h>
#include <string>

void VideopacFont_DrawText(
    SDL_Renderer* renderer,
    float x,
    float y,
    float scale,
    const std::string& text);
