#include "library_view.h"

std::vector<RomEntry> BuildLibraryView(
    const std::vector<RomEntry>& allRoms,
    LibraryView view)
{
    std::vector<RomEntry> result;
    result.reserve(allRoms.size());

    for (const RomEntry& rom : allRoms)
    {
        const bool includeRom =
            (view == LibraryView::Official)
                ? rom.info.official
                : !rom.info.official;

        if (includeRom)
        {
            result.push_back(rom);
        }
    }

    return result;
}
