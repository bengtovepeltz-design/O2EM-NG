#include "settings.h"

#include <fstream>
#include <sstream>
#include <algorithm>

static std::string Trim(std::string s)
{
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
        }));

    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
        }).base(), s.end());

    return s;
}

static std::string ToLower(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
        });
    return s;
}

static bool ParseBool(const std::string& value, bool defaultValue)
{
    std::string v = ToLower(Trim(value));

    if (v == "true" || v == "1" || v == "yes" || v == "on")
        return true;

    if (v == "false" || v == "0" || v == "no" || v == "off")
        return false;

    return defaultValue;
}

std::string RegionModeToString(RegionMode mode)
{
    switch (mode)
    {
    case RegionMode::PAL:
        return "pal";
    case RegionMode::NTSC:
        return "ntsc";
    case RegionMode::Auto:
    default:
        return "auto";
    }
}

RegionMode RegionModeFromString(const std::string& value)
{
    std::string v = ToLower(Trim(value));

    if (v == "pal")
        return RegionMode::PAL;

    if (v == "ntsc")
        return RegionMode::NTSC;

    return RegionMode::Auto;
}

O2EMSettings LoadSettings(const std::string& filename)
{
    O2EMSettings settings;

    std::ifstream file(filename);

    if (!file.is_open())
    {
        SaveSettings(filename, settings);
        return settings;
    }

    std::string line;

    while (std::getline(file, line))
    {
        line = Trim(line);

        if (line.empty() || line[0] == '#')
            continue;

        size_t equalsPos = line.find('=');

        if (equalsPos == std::string::npos)
            continue;

        std::string key = Trim(line.substr(0, equalsPos));
        std::string value = Trim(line.substr(equalsPos + 1));

        key = ToLower(key);

        if (key == "start_fullscreen")
        {
            settings.start_fullscreen = ParseBool(value, settings.start_fullscreen);
        }
        else if (key == "region_mode")
        {
            settings.region_mode = RegionModeFromString(value);
        }
        else if (key == "scanlines")
        {
            settings.scanlines = ParseBool(value, settings.scanlines);
        }
    }

    return settings;
}

void SaveSettings(const std::string& filename, const O2EMSettings& settings)
{
    std::ofstream file(filename);

    if (!file.is_open())
        return;

    file << "# O2EM-NG configuration\n";
    file << "# Values: true/false, auto/pal/ntsc\n\n";

    file << "start_fullscreen = " << (settings.start_fullscreen ? "true" : "false") << "\n";
    file << "region_mode = " << RegionModeToString(settings.region_mode) << "\n";
    file << "scanlines = " << (settings.scanlines ? "true" : "false") << "\n";
}