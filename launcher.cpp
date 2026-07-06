#include "launcher.h"
#include "emulator_core.h"

void LaunchRom(SDL_Window* window, const RomEntry& rom, RegionMode regionMode)
{
    EmulatorCore_StartRom(rom.path, regionMode);
}