#pragma once

#include <SDL3/SDL.h>

#include "settings.h"

struct GameInfo;

// Returns true when the emulator session ran to a normal game exit, false when
// the machine could not be started (BIOS/ROM load failure). The caller owns
// the surrounding frontend lifecycle either way.
bool LaunchRom(
    SDL_Window* window,
    const GameInfo& game,
    RegionMode regionMode,
    const std::string& biosFile,
    bool scanlines);
