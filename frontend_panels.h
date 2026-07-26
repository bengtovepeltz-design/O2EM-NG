#pragma once

#include <SDL3/SDL.h>

struct FrontendPanelLayout
{
    SDL_FRect leftOuter;
    SDL_FRect leftContent;
    SDL_FRect rightOuter;
    SDL_FRect rightContent;
};

FrontendPanelLayout FrontendPanels_Calculate(int windowWidth, int windowHeight);
void FrontendPanels_Draw(
    SDL_Renderer* renderer,
    const FrontendPanelLayout& layout);
