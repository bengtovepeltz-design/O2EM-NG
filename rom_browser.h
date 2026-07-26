#pragma once

#include <string>
#include <vector>

#include "src/database/rom_classifier.h"

struct RomEntry
{
    std::string name;
    std::string path;
    RomClassification info;
};

std::vector<RomEntry> LoadRoms(const std::string& romFolder);
