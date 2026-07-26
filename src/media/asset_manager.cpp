#include "asset_manager.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <system_error>
#include <vector>

#include "../library/game_info.h"
#include "../library/game_library.h"

namespace
{
    using Path = std::filesystem::path;

    const char* ImageExtensions[] =
    {
        ".jpg", ".png", ".jpeg", ".bmp", ".webp"
    };

    const char* ManualExtensions[] =
    {
        ".pdf", ".txt", ".html", ".htm"
    };

    bool IsRegularFile(const Path& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    bool IsDirectory(const Path& path)
    {
        std::error_code error;
        return std::filesystem::is_directory(path, error) && !error;
    }

    std::string ToLower(std::string value)
    {
        std::transform(
            value.begin(),
            value.end(),
            value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });

        return value;
    }

    bool HasExtension(
        const Path& path,
        const char* const* extensions,
        std::size_t extensionCount)
    {
        const std::string extension = ToLower(path.extension().string());

        for (std::size_t index = 0; index < extensionCount; ++index)
        {
            if (extension == extensions[index])
                return true;
        }

        return false;
    }

    void AddCandidates(
        std::vector<Path>& candidates,
        const Path& folder,
        const std::string& baseName,
        const char* const* extensions,
        std::size_t extensionCount)
    {
        if (baseName.empty())
            return;

        for (std::size_t index = 0; index < extensionCount; ++index)
            candidates.push_back(folder / (baseName + extensions[index]));
    }

    Path FindFirstExisting(const std::vector<Path>& candidates)
    {
        for (const Path& candidate : candidates)
        {
            if (IsRegularFile(candidate))
                return candidate;
        }

        return {};
    }

    std::string TwoDigitNumber(int number)
    {
        if (number <= 0 || number > 99)
            return {};

        std::string value = std::to_string(number);
        if (value.size() == 1)
            value.insert(value.begin(), '0');

        return value;
    }

    bool IsPlusVariant(const GameInfo& game)
    {
        const std::string stem = ToLower(game.romPath.stem().string());
        return stem.find("pl") != std::string::npos ||
               stem.find("_12") != std::string::npos ||
               stem.find("_16") != std::string::npos;
    }

    void AddOfficialBoxCandidates(
        std::vector<Path>& candidates,
        const Path& folder,
        const GameInfo& game)
    {
        const std::string number = TwoDigitNumber(game.videopacNumber);
        if (number.empty())
            return;

        const bool plusVariant = IsPlusVariant(game);

        const char* regularSuffixes[] =
        {
            "_plastic_front",
            "_plastic_cover",
            "_cardboard_front",
            "_canada_front",
            "_us_cardboard_front",
            "_radiola_plastic_front",
            "_jopac_plastic_front"
        };

        const char* plusSuffixes[] =
        {
            "plus_plastic_front",
            "plus_plastic_cover",
            "plus_cardboard_front",
            "plus_jopac_plastic_front"
        };

        auto addRegular = [&]()
        {
            for (const char* suffix : regularSuffixes)
            {
                AddCandidates(
                    candidates,
                    folder,
                    number + suffix,
                    ImageExtensions,
                    std::size(ImageExtensions));
            }
        };

        auto addPlus = [&]()
        {
            for (const char* suffix : plusSuffixes)
            {
                AddCandidates(
                    candidates,
                    folder,
                    number + suffix,
                    ImageExtensions,
                    std::size(ImageExtensions));
            }
        };

        if (plusVariant)
        {
            addPlus();
            addRegular();
        }
        else
        {
            addRegular();
            addPlus();
        }
    }

    Path FindImage(
        const Path& folder,
        const GameInfo& game,
        bool includeOfficialBoxNames)
    {
        if (!IsDirectory(folder))
            return {};

        std::vector<Path> candidates;

        if (includeOfficialBoxNames)
            AddOfficialBoxCandidates(candidates, folder, game);

        AddCandidates(
            candidates,
            folder,
            game.romPath.stem().string(),
            ImageExtensions,
            std::size(ImageExtensions));

        AddCandidates(
            candidates,
            folder,
            game.filename.empty()
                ? std::string()
                : Path(game.filename).stem().string(),
            ImageExtensions,
            std::size(ImageExtensions));

        return FindFirstExisting(candidates);
    }

    Path FindManualFile(const Path& folder, const GameInfo& game)
    {
        if (!IsDirectory(folder))
            return {};

        const std::string number = TwoDigitNumber(game.videopacNumber);
        if (number.empty())
            return {};

        // Manuals use the cartridge ID as their stable key, for example:
        //   ROM:    vp_37.bin
        //   Manual: 37_philips_manual.pdf
        // Match only the leading two-digit ID and ignore the descriptive
        // remainder of the filename.
        const std::string requiredPrefix = ToLower(number + "_");
        std::vector<Path> matches;

        std::error_code error;
        for (std::filesystem::directory_iterator iterator(folder, error), end;
             !error && iterator != end;
             iterator.increment(error))
        {
            const Path path = iterator->path();
            if (!IsRegularFile(path) ||
                !HasExtension(path, ManualExtensions, std::size(ManualExtensions)))
            {
                continue;
            }

            const std::string filename = ToLower(path.filename().string());
            if (filename.rfind(requiredPrefix, 0) == 0)
                matches.push_back(path);
        }

        if (matches.empty())
            return {};

        std::sort(matches.begin(), matches.end());
        return matches.front();
    }

    std::vector<Path> FindScreenshots(const Path& folder, const GameInfo& game)
    {
        std::vector<Path> screenshots;
        if (!IsDirectory(folder))
            return screenshots;

        const std::string stem = ToLower(game.romPath.stem().string());
        const std::string number = TwoDigitNumber(game.videopacNumber);
        const Path gameFolder = folder / game.romPath.stem();

        auto collectDirectory = [&](const Path& directory)
        {
            if (!IsDirectory(directory))
                return;

            std::error_code error;
            for (std::filesystem::directory_iterator iterator(directory, error), end;
                 !error && iterator != end;
                 iterator.increment(error))
            {
                const Path path = iterator->path();
                if (IsRegularFile(path) &&
                    HasExtension(path, ImageExtensions, std::size(ImageExtensions)))
                {
                    screenshots.push_back(path);
                }
            }
        };

        collectDirectory(gameFolder);

        std::error_code error;
        for (std::filesystem::directory_iterator iterator(folder, error), end;
             !error && iterator != end;
             iterator.increment(error))
        {
            const Path path = iterator->path();
            if (!IsRegularFile(path) ||
                !HasExtension(path, ImageExtensions, std::size(ImageExtensions)))
            {
                continue;
            }

            const std::string candidateStem = ToLower(path.stem().string());
            const bool matchesRom =
                !stem.empty() && candidateStem.rfind(stem, 0) == 0;
            const bool matchesNumber =
                !number.empty() &&
                (candidateStem.rfind(number, 0) == 0 ||
                 candidateStem.rfind("vp_" + number, 0) == 0);

            if (matchesRom || matchesNumber)
                screenshots.push_back(path);
        }

        std::sort(screenshots.begin(), screenshots.end());
        screenshots.erase(
            std::unique(screenshots.begin(), screenshots.end()),
            screenshots.end());

        return screenshots;
    }
}

AssetManager::AssetManager(const std::filesystem::path& basePath)
{
    SetBasePath(basePath);
}

void AssetManager::SetBasePath(const std::filesystem::path& basePath)
{
    basePath_ = basePath.empty()
        ? std::filesystem::current_path()
        : basePath;
}

const std::filesystem::path& AssetManager::BasePath() const noexcept
{
    return basePath_;
}

std::filesystem::path AssetManager::FindBoxArt(const GameInfo& game) const
{
    return FindImage(basePath_ / "BOXART", game, true);
}

std::filesystem::path AssetManager::FindCartridge(const GameInfo& game) const
{
    Path path = FindImage(basePath_ / "CARTRIDGES", game, false);
    if (path.empty())
        path = FindImage(basePath_ / "CARTRIDGE", game, false);

    return path;
}

std::filesystem::path AssetManager::FindManual(const GameInfo& game) const
{
    Path path = FindManualFile(basePath_ / "MANUALS", game);
    if (path.empty())
        path = FindManualFile(basePath_ / "MANUAL", game);

    return path;
}

void AssetManager::Populate(GameInfo& game) const
{
    game.boxArt = FindBoxArt(game);
    game.cartridge = FindCartridge(game);
    game.manual = FindManual(game);
    game.screenshots = FindScreenshots(basePath_ / "SCREENSHOTS", game);
}

void AssetManager::Populate(GameLibrary& library) const
{
    for (GameInfo& game : library.Games())
        Populate(game);
}
