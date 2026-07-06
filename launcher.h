#pragma once

#include <SDL3/SDL.h>
#include "rom_browser.h"
#include "settings.h"

void LaunchRom(SDL_Window* window, const RomEntry& rom, RegionMode regionMode);