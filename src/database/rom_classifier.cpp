#include "rom_classifier.h"

#include <algorithm>
#include <cctype>
#include <regex>

namespace
{
    std::string ToLower(std::string text)
    {
        std::transform(
            text.begin(),
            text.end(),
            text.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(
                    std::tolower(character));
            });

        return text;
    }

    bool StartsWith(
        const std::string& text,
        const std::string& prefix)
    {
        return text.rfind(prefix, 0) == 0;
    }

    bool Contains(
        const std::string& text,
        const std::string& value)
    {
        return text.find(value) != std::string::npos;
    }

    bool IsOfficialVideopacRom(
        const std::string& romId)
    {
        static const std::regex officialPattern(
            R"(^vp_(0[1-9]|[1-5][0-9]|60)(?:_12|_16)?$)",
            std::regex_constants::icase);

        return std::regex_match(
            romId,
            officialPattern);
    }

    RomCategory DetectCategory(
        const std::string& romId)
    {
        if (IsOfficialVideopacRom(romId))
            return RomCategory::Official;

        if (StartsWith(romId, "bios_"))
            return RomCategory::Bios;

        if (StartsWith(romId, "br_"))
            return RomCategory::Brazilian;

        if (StartsWith(romId, "im_"))
            return RomCategory::Imagic;

        if (StartsWith(romId, "jo_"))
            return RomCategory::Jopac;

        if (StartsWith(romId, "mod_"))
            return RomCategory::Modified;

        if (StartsWith(romId, "new_"))
            return RomCategory::Homebrew;

        if (StartsWith(romId, "ntsc_"))
            return RomCategory::Ntsc;

        if (StartsWith(romId, "o2_"))
            return RomCategory::Odyssey2;

        if (StartsWith(romId, "pal_"))
            return RomCategory::Pal;

        if (StartsWith(romId, "pb_"))
            return RomCategory::ParkerBrothers;

        if (StartsWith(romId, "pr_"))
            return RomCategory::Prototype;

        return RomCategory::Other;
    }

    std::string CategoryToString(
        RomCategory category)
    {
        switch (category)
        {
        case RomCategory::Official:
            return "Official Videopac";

        case RomCategory::Brazilian:
            return "Brazilian";

        case RomCategory::Imagic:
            return "Imagic";

        case RomCategory::Jopac:
            return "Jopac";

        case RomCategory::Modified:
            return "Modified";

        case RomCategory::Homebrew:
            return "Homebrew";

        case RomCategory::Ntsc:
            return "NTSC";

        case RomCategory::Odyssey2:
            return "Odyssey2";

        case RomCategory::Pal:
            return "PAL";

        case RomCategory::ParkerBrothers:
            return "Parker Brothers";

        case RomCategory::Prototype:
            return "Prototype";

        case RomCategory::Bios:
            return "BIOS";

        default:
            return "Other";
        }
    }
}

RomClassification ClassifyRom(
    const std::filesystem::path& romPath)
{
    RomClassification result;

    result.romId =
        ToLower(romPath.stem().string());

    result.category =
        DetectCategory(result.romId);

    result.displayCategory =
        CategoryToString(result.category);

    result.official =
        result.category == RomCategory::Official;

    result.plus =
        Contains(result.romId, "_pl") ||
        Contains(result.romId, "pl");

    result.alternate =
        Contains(result.romId, "alt");

    result.hack =
        Contains(result.romId, "hack");

    result.fixed =
        Contains(result.romId, "fix");

    result.french =
        Contains(result.romId, "_f");

    result.cartridge12k =
        Contains(result.romId, "_12");

    result.cartridge16k =
        Contains(result.romId, "_16");

    result.requiresG7400OrJopac =
        result.romId == "vp_56pl" ||
        result.romId == "vp_59_16" ||
        result.romId == "vp_60_16" ||
        result.romId == "jo_moto-crash_pl";

    return result;
}  