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
    // Editable European catalogue identity, e.g. 10, 11+, 54+, C7010.
    // This is metadata; rom_filename remains the stable database key.
    std::string catalogId;

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
    // Personal user rating: 0 = not rated, 1..5 = stars. Stored in the
    // separate games.user_rating column; unrelated to the legacy free-text
    // `rating` metadata field above, which is preserved for compatibility.
    int userRating = 0;
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
std::string ParseVideopacCatalogIdFromFilename(const std::string& filename);

// True when a catalogue ID and a ROM filename denote the SAME entry
// (e.g. "54+" <-> vp_54+.bin). Catalogue identity includes the '+' variant:
// 54 and 54+ are independent entries and neither implies the other. Used to
// keep the library, Game Information and the actual ROM directory in exact
// agreement when coloring the catalogue number.
bool CatalogIdMatchesFilename(const std::string& catalogId,
    const std::string& romFilename);

GameInfo MakeGameInfo(const RomEntry& rom);

// Fills only missing fields with conservative, clearly generic catalog data.
// Imported/user metadata always remains authoritative.
void ApplyCatalogFallbacks(GameInfo& game);
