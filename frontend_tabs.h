#pragma once

#include <SDL3/SDL.h>

enum class FrontendTab
{
    Library = 0,
    Cartridge,
    Extras,
    Screenshot,
    Manual,
    Settings,
    About,
    Credits,
    Count
};

int FrontendTabs_GetCount() noexcept;
const char* FrontendTabs_GetName(FrontendTab tab) noexcept;

void FrontendTabs_Draw(
    SDL_Renderer* renderer,
    int windowWidth,
    FrontendTab activeTab);
