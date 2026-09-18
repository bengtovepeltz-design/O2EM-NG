#include "game_info.h"

#include <cctype>
#include <string>
#include <string.h>

int ParseVideopacNumberFromFilename(const std::string& romId)
{
        // The official cartridge ID is the two-digit number immediately
        // following the vp_ prefix. Suffixes such as _12, _16, alt or hack
        // do not change the ID used by manuals and other media.
        if (romId.size() < 5)
            return 0;

        std::string lower = romId;
        for (char& character : lower)
        {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }

        if (lower.rfind("vp_", 0) != 0)
            return 0;

        const unsigned char first = static_cast<unsigned char>(lower[3]);
        const unsigned char second = static_cast<unsigned char>(lower[4]);
        if (!std::isdigit(first) || !std::isdigit(second))
            return 0;

    const int number = (lower[3] - '0') * 10 + (lower[4] - '0');
    return number >= 1 && number <= 99 ? number : 0;
}

std::string ParseVideopacCatalogIdFromFilename(const std::string& filename)
{
    std::string lower = filename;
    for (char& c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    if (lower.rfind("vp_", 0) == 0 && lower.size() >= 5 &&
        std::isdigit(static_cast<unsigned char>(lower[3])) &&
        std::isdigit(static_cast<unsigned char>(lower[4])))
    {
        // Patch 0031: catalogue IDs must keep the '+' variant as part of the
        // identity. The master catalogue (Gamelist.txt) spells Videopac+
        // cartridges as vp_NNpl.bin, vp_NN_12.bin, vp_NN_12fix.bin or
        // vp_NN_16.bin; installed ROMs usually use vp_NN+.bin. All of these
        // denote the SAME catalogue identity (54+), never the plain number.
        std::string id = filename.substr(3, 2);
        const std::string lowerSuffix = lower.substr(5);
        const bool plusVariant =
            (filename.size() > 5 && filename[5] == '+') ||
            lowerSuffix.rfind("pl", 0) == 0 ||          // vp_54pl.bin
            lowerSuffix.rfind("_12", 0) == 0 ||         // vp_55_12.bin / _12fix
            lowerSuffix.rfind("_16", 0) == 0;           // vp_59_16.bin / vp_60_16.bin
        if (plusVariant) id += "+";
        return id;
    }
    // Expansion modules may use their hardware designation as catalogue ID.
    if (lower.find("c7010") != std::string::npos) return "C7010";
    if (lower.find("c7420") != std::string::npos) return "C7420";
    return {};
}

bool CatalogIdMatchesFilename(const std::string& catalogId,
    const std::string& romFilename)
{
    // Derive the catalogue ID the file itself denotes and compare exactly.
    // '+' is part of the identity: 54 != 54+. Canonical catalogue spellings
    // (vp_54pl.bin, vp_55_12.bin, vp_59_16.bin, vp_54+.bin) all map to the
    // same 54+/55+ style ID as their user-visible filename spellings.
    if (catalogId.empty() || romFilename.empty())
        return false;
    return _stricmp(ParseVideopacCatalogIdFromFilename(romFilename).c_str(),
        catalogId.c_str()) == 0;
}

GameInfo MakeGameInfo(const RomEntry& rom)
{
    GameInfo game;

    game.rom = rom;
    game.videopacNumber = ParseVideopacNumberFromFilename(rom.info.romId);
    game.catalogId = ParseVideopacCatalogIdFromFilename(rom.info.romId);
    game.title = rom.name;
    game.sortTitle = rom.name;
    game.genre = rom.info.displayCategory;
    game.videopacPlus = rom.info.plus ? "Yes" : "No";
    game.romPath = std::filesystem::path(rom.path);
    game.filename = game.romPath.filename().string();

    return game;
}


void ApplyCatalogFallbacks(GameInfo& game)
{
    // This is deliberately conservative. Gamelist.txt provides titles and
    // categories, but not complete release metadata. Never overwrite values
    // supplied by METADATA files or the user database.
    const bool officialVideopac =
        game.videopacNumber > 0 &&
        (game.genre.empty() ||
         game.genre == "Official Videopac" ||
         game.genre == "Videopac");

    if (!officialVideopac)
        return;

    // Patch 0012b: Gamelist.txt uses a broad catalogue category. Replace it
    // with a useful game genre where we have a verified cartridge-specific
    // classification. User-edited metadata is never overwritten.
    if (game.videopacNumber == 40 &&
        (game.genre.empty() ||
         game.genre == "Official Videopac" ||
         game.genre == "Videopac"))
    {
        game.genre = "Board Game";
    }
    else if (game.videopacNumber == 58 &&
        (game.genre.empty() ||
         game.genre == "Official Videopac" ||
         game.genre == "Videopac"))
    {
        game.genre = "Shooter";
    }

    if (game.publisher.empty())
        game.publisher = "Philips";

    if (game.players.empty())
        game.players = "See original manual";

    if (game.controls.empty())
        game.controls = "Joystick / game-specific controls";

    if (game.shortDescription.empty())
    {
        game.shortDescription =
            "Official Philips Videopac G7000 cartridge #" +
            std::to_string(game.videopacNumber) + ".";
    }

    if (game.description.empty())
    {
        game.description = game.title +
            " is an official Philips Videopac G7000 cartridge. "
            "Open the Manual tab for the original game instructions.";
    }

    if (game.voiceModule.empty())
        game.voiceModule = "See original manual";

    if (game.rating.empty())
        game.rating = "Not rated";
}
