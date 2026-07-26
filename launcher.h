#pragma once

#include <SDL3/SDL.h>

#include "settings.h"

struct GameInfo;

void LaunchRom(
    SDL_Window* window,
    const GameInfo& game,
    RegionMode regionMode,
    const std::string& biosFile,
    bool scanlines);
