#pragma once

#include <filesystem>
#include <string>

enum class RomCategory
{
    Official,
    Brazilian,
    Imagic,
    Jopac,
    Modified,
    Homebrew,
    Ntsc,
    Odyssey2,
    Pal,
    ParkerBrothers,
    Prototype,
    Bios,
    Other
};

struct RomClassification
{
    RomCategory category = RomCategory::Other;

    std::string romId;
    std::string displayCategory;

    bool official = false;
    bool plus = false;
    bool alternate = false;
    bool hack = false;
    bool fixed = false;
    bool french = false;
    bool cartridge12k = false;
    bool cartridge16k = false;
    bool requiresG7400OrJopac = false;
};

RomClassification ClassifyRom(
    const std::filesystem::path& romPath);