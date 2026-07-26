#pragma once

#include <ctime>
#include <filesystem>
#include <string>
#include <vector>

#include "../../rom_browser.h"

// Complete collection-facing description of one game.
struct GameInfo
{
    int videopacNumber = 0;

    std::string title;
    std::string sortTitle;
    std::string filename;

    std::string year;
    std::string publisher;
    std::string developer;
    std::string genre;
    std::string players;
    std::string controls;
    std::string voiceModule;
    std::string videopacPlus;
    std::string rating;
    std::string shortDescription;
    std::string description;
    std::string trivia;

    std::filesystem::path romPath;
    std::filesystem::path boxArt;
    std::filesystem::path cartridge;
    std::filesystem::path manual;
    std::vector<std::filesystem::path> screenshots;

    bool favorite = false;
    int playCount = 0;
    std::time_t lastPlayed = 0;

    RomEntry rom;
};

// Returns the official two-digit Videopac number from filenames such as
// vp_40.bin, or 0 when the filename does not contain a valid official ID.
int ParseVideopacNumberFromFilename(const std::string& filename);

GameInfo MakeGameInfo(const RomEntry& rom);

// Fills only missing fields with conservative, clearly generic catalog data.
// Imported/user metadata always remains authoritative.
void ApplyCatalogFallbacks(GameInfo& game);
