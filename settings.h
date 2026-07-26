#pragma once

#include <string>

enum class RegionMode
{
    Auto,
    PAL,
    NTSC
};

struct O2EMSettings
{
    bool start_fullscreen = true;
    RegionMode region_mode = RegionMode::Auto;
    bool scanlines = false;
    std::string bios_file = "g7000.bin";
};

O2EMSettings LoadSettings(const std::string& filename);
void SaveSettings(const std::string& filename, const O2EMSettings& settings);

std::string RegionModeToString(RegionMode mode);
RegionMode RegionModeFromString(const std::string& value);