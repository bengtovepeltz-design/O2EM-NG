#include "media_resolver.h"

#include <filesystem>
#include <regex>
#include <string>
#include <vector>

namespace
{
    bool IsOfficialVideopacRom(
        const std::filesystem::path& romPath)
    {
        static const std::regex pattern(
            R"(vp_(\d{2}))",
            std::regex_constants::icase);

        return std::regex_search(
            romPath.stem().string(),
            pattern);
    }

    std::string GetOfficialGameNumber(
        const std::filesystem::path& romPath)
    {
        static const std::regex pattern(
            R"(vp_(\d{2}))",
            std::regex_constants::icase);

        std::smatch match;
        const std::string romStem =
            romPath.stem().string();

        if (std::regex_search(
                romStem,
                match,
                pattern))
        {
            return match[1].str();
        }

        return {};
    }

    bool IsPlusVariant(
        const std::filesystem::path& romPath)
    {
        const std::string romStem =
            romPath.stem().string();

        return romStem.find("pl") != std::string::npos ||
               romStem.find("PL") != std::string::npos ||
               romStem.find("_12") != std::string::npos ||
               romStem.find("_16") != std::string::npos;
    }

    void AddImageCandidate(
        std::vector<std::filesystem::path>& candidates,
        const std::filesystem::path& boxArtFolder,
        const std::string& baseName)
    {
        static const char* extensions[] =
        {
            ".jpg",
            ".png",
            ".jpeg",
            ".bmp"
        };

        for (const char* extension : extensions)
        {
            candidates.push_back(
                boxArtFolder / (baseName + extension));
        }
    }

    void AddRegularArtworkCandidates(
        std::vector<std::filesystem::path>& candidates,
        const std::filesystem::path& boxArtFolder,
        const std::string& gameNumber)
    {
        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_plastic_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_plastic_cover");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_cardboard_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_canada_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_us_cardboard_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_radiola_plastic_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "_jopac_plastic_front");
    }

    void AddPlusArtworkCandidates(
        std::vector<std::filesystem::path>& candidates,
        const std::filesystem::path& boxArtFolder,
        const std::string& gameNumber)
    {
        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "plus_plastic_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "plus_plastic_cover");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "plus_cardboard_front");

        AddImageCandidate(
            candidates,
            boxArtFolder,
            gameNumber + "plus_jopac_plastic_front");
    }

    std::filesystem::path FindFirstExisting(
        const std::vector<std::filesystem::path>& candidates)
    {
        for (const auto& candidate : candidates)
        {
            if (std::filesystem::exists(candidate) &&
                std::filesystem::is_regular_file(candidate))
            {
                return candidate;
            }
        }

        return {};
    }
}

namespace MediaResolver
{
    std::filesystem::path FindBoxArt(
        const std::filesystem::path& romPath)
    {
        const std::filesystem::path boxArtFolder =
            std::filesystem::current_path() / "BOXART";

        if (!std::filesystem::exists(boxArtFolder))
        {
            return {};
        }

        const std::string romStem =
            romPath.stem().string();

        if (IsOfficialVideopacRom(romPath))
        {
            const std::string gameNumber =
                GetOfficialGameNumber(romPath);

            if (gameNumber.empty())
            {
                return {};
            }

            std::vector<std::filesystem::path> candidates;

            if (IsPlusVariant(romPath))
            {
                AddPlusArtworkCandidates(
                    candidates,
                    boxArtFolder,
                    gameNumber);

                AddRegularArtworkCandidates(
                    candidates,
                    boxArtFolder,
                    gameNumber);
            }
            else
            {
                AddRegularArtworkCandidates(
                    candidates,
                    boxArtFolder,
                    gameNumber);

                AddPlusArtworkCandidates(
                    candidates,
                    boxArtFolder,
                    gameNumber);
            }

            AddImageCandidate(
                candidates,
                boxArtFolder,
                romStem);

            return FindFirstExisting(candidates);
        }

        std::vector<std::filesystem::path> candidates;

        AddImageCandidate(
            candidates,
            boxArtFolder,
            romStem);

        return FindFirstExisting(candidates);
    }
}
