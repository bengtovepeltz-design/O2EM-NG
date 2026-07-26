#pragma once

#include <SDL3/SDL.h>

struct GameInfo;

void FrontendBoxArt_DrawImage(
    SDL_Renderer* renderer,
    const SDL_FRect& imageArea,
    const GameInfo* game);

void FrontendBoxArt_Draw(
    SDL_Renderer* renderer,
    const SDL_FRect& rightContent,
    const GameInfo* game);

void FrontendBoxArt_Shutdown();
