#pragma once

#include <SDL3/SDL.h>

void FrontendStatusBar_Draw(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    int gameCount,
    const char* statusText);

bool FrontendStatusBar_HitTestExit(
    int windowWidth,
    int windowHeight,
    float x,
    float y) noexcept;
