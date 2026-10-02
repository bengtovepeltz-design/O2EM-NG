#pragma once

#include <SDL3/SDL.h>

enum class FrontendTab
{
    Library = 0,
    Cartridge,
    Extras,
    Manual,
    Settings,
    About,
    // "Credits" is no longer a top-level tab: About already owns the internal
    // About / Credits / Special Thanks / Roadmap / Release Notes pages. The
    // freed top-level slot now opens the dedicated My Collection page.
    MyCollection,
    Count
};

int FrontendTabs_GetCount() noexcept;
const char* FrontendTabs_GetName(FrontendTab tab) noexcept;

void FrontendTabs_Draw(
    SDL_Renderer* renderer,
    int windowWidth,
    FrontendTab activeTab);
