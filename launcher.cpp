#include "launcher.h"

#include "emulator_core.h"
#include "src/library/game_info.h"

void LaunchRom(
    SDL_Window* window,
    const GameInfo& game,
    RegionMode regionMode,
    const std::string& biosFile,
    bool scanlines)
{
    const std::string romPath = game.romPath.string();
    EmulatorCore_StartRom(
        romPath,
        regionMode,
        biosFile,
        scanlines);
}
