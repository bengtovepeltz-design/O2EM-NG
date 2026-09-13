#include "rom_browser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <utility>

std::vector<RomEntry> LoadRoms(const std::string& romFolder)
{
    std::vector<RomEntry> roms;

    if (!std::filesystem::exists(romFolder))
    {
        return roms;
    }

    for (const auto& entry :
        std::filesystem::directory_iterator(romFolder))
    {
        if (!entry.is_regular_file())
        {
            continue;
        }

        const std::filesystem::path path = entry.path();
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

        if (extension != ".bin" && extension != ".rom")
        {
            continue;
        }

        RomEntry rom;
        rom.name = path.stem().string();
        rom.path = path.string();
        rom.info = ClassifyRom(path);

        roms.push_back(std::move(rom));
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
