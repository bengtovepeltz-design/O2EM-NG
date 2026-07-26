#pragma once

#include <vector>

#include "../../rom_browser.h"

enum class LibraryView
{
    Official,
    Extras
};

std::vector<RomEntry> BuildLibraryView(
    const std::vector<RomEntry>& allRoms,
    LibraryView view);
