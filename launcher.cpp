#include "launcher.h"

#include "emulator_core.h"
#include "src/library/game_info.h"
#include <cstdio>

namespace
{
    // AUTO follows the cartridge's native region instead of the legacy
    // always-NTSC fallback. European Videopac/Jopac cartridges are PAL; the
    // US Odyssey2, explicit NTSC and Brazilian (PAL-M/60 Hz) sets are NTSC.
    // The normalized romId is used because official Videopac+ spellings such
    // as vp_59+ are not part of the classifier's Official category pattern.
    // Unknown, modified, prototype and homebrew entries keep the legacy
    // behavior so this change only resolves regions we can prove.
    RegionMode ResolveAutoRegion(const GameInfo& game, RegionMode requested)
    {
        if (requested != RegionMode::Auto)
            return requested;

        const std::string& romId = game.rom.info.romId;

        if (romId.rfind("pal_", 0) == 0 ||
            romId.rfind("jo_", 0) == 0 ||
            romId.rfind("vp_", 0) == 0)
            return RegionMode::PAL;

        if (romId.rfind("ntsc_", 0) == 0 ||
            romId.rfind("br_", 0) == 0 ||
            romId.rfind("o2_", 0) == 0)
            return RegionMode::NTSC;

        return requested;
    }
}

bool LaunchRom(
    SDL_Window* window,
    const GameInfo& game,
    RegionMode regionMode,
    const std::string& biosFile,
    bool scanlines)
{
    const std::string romPath = game.romPath.string();
    const RegionMode effectiveRegion = ResolveAutoRegion(game, regionMode);

#if defined(_DEBUG)
    if (effectiveRegion != regionMode)
    {
        std::printf("O2EM-NG: AUTO region resolved to %s for %s\n",
            effectiveRegion == RegionMode::PAL ? "PAL" : "NTSC",
            game.filename.c_str());
        std::fflush(stdout);
    }
#endif

    return EmulatorCore_StartRom(
        romPath,
        effectiveRegion,
        biosFile,
        scanlines);
}
