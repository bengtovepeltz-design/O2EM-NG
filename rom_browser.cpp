#include "rom_browser.h"

#include <filesystem>
#include <algorithm>

std::vector<RomEntry> LoadRoms(const std::string& romFolder)
{
    std::vector<RomEntry> roms;

    if (!std::filesystem::exists(romFolder))
        return roms;

    for (const auto& entry : std::filesystem::directory_iterator(romFolder))
    {
        if (!entry.is_regular_file())
            continue;

        auto path = entry.path();

        if (path.extension() == ".bin" || path.extension() == ".BIN")
        {
            roms.push_back({
                path.stem().string(),
                path.string()
                });
        }
    }

    std::sort(
        roms.begin(),
        roms.end(),
        [](const RomEntry& a, const RomEntry& b)
        {
            return a.name < b.name;
        });

    return roms;
}