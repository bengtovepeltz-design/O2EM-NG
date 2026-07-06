#pragma once

#include <string>
#include <vector>

struct RomEntry
{
    std::string name;
    std::string path;
};

std::vector<RomEntry> LoadRoms(const std::string& romFolder);