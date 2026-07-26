#include "metadata_engine.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#include "../library/game_info.h"
#include "../library/game_library.h"

namespace
{
    using Path = std::filesystem::path;

    std::string Trim(const std::string& value)
    {
        const auto first = std::find_if_not(
            value.begin(), value.end(),
            [](unsigned char character) { return std::isspace(character) != 0; });

        if (first == value.end())
            return {};

        const auto last = std::find_if_not(
            value.rbegin(), value.rend(),
            [](unsigned char character) { return std::isspace(character) != 0; }).base();

        return std::string(first, last);
    }

    std::string ToLower(std::string value)
    {
        std::transform(
            value.begin(), value.end(), value.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        return value;
    }

    bool IsRegularFile(const Path& path)
    {
        std::error_code error;
        return std::filesystem::is_regular_file(path, error) && !error;
    }

    std::string TwoDigitNumber(int number)
    {
        if (number <= 0 || number > 99)
            return {};

        std::string result = std::to_string(number);
        if (result.size() == 1)
            result.insert(result.begin(), '0');
        return result;
    }


    std::string DecodeEscapes(const std::string& value)
    {
        std::string result;
        result.reserve(value.size());

        for (std::size_t index = 0; index < value.size(); ++index)
        {
            if (value[index] == '\\' && index + 1 < value.size())
            {
                const char next = value[index + 1];
                if (next == 'n')
                {
                    result.push_back('\n');
                    ++index;
                    continue;
                }
                if (next == 't')
                {
                    result.push_back('\t');
                    ++index;
                    continue;
                }
                if (next == '\\')
                {
                    result.push_back('\\');
                    ++index;
                    continue;
                }
            }

            result.push_back(value[index]);
        }

        return result;
    }

    void ApplyValue(GameInfo& game, const std::string& rawKey, const std::string& rawValue)
    {
        const std::string key = ToLower(Trim(rawKey));
        const std::string value = DecodeEscapes(Trim(rawValue));

        if (key == "title" && !value.empty())
            game.title = value;
        else if (key == "sort_title")
            game.sortTitle = value;
        else if (key == "year")
            game.year = value;
        else if (key == "genre")
            game.genre = value;
        else if (key == "publisher")
            game.publisher = value;
        else if (key == "developer")
            game.developer = value;
        else if (key == "players")
            game.players = value;
        else if (key == "controller" || key == "controls")
            game.controls = value;
        else if (key == "voice_module" || key == "voice")
            game.voiceModule = value;
        else if (key == "videopac_plus" || key == "plus")
            game.videopacPlus = value;
        else if (key == "rating")
            game.rating = value;
        else if (key == "short_description")
            game.shortDescription = value;
        else if (key == "description")
            game.description = value;
        else if (key == "trivia")
            game.trivia = value;
        else if (key == "manual" || key == "manual_path")
        {
            const Path manualPath(value);
            game.manual = manualPath.is_absolute() ? manualPath : game.romPath.parent_path().parent_path() / manualPath;
        }
    }
}

MetadataEngine::MetadataEngine(const std::filesystem::path& basePath)
{
    SetBasePath(basePath);
}

void MetadataEngine::SetBasePath(const std::filesystem::path& basePath)
{
    basePath_ = basePath.empty()
        ? std::filesystem::current_path()
        : basePath;
}

const std::filesystem::path& MetadataEngine::BasePath() const noexcept
{
    return basePath_;
}

std::filesystem::path MetadataEngine::FindMetadata(const GameInfo& game) const
{
    const Path metadataFolder = basePath_ / "METADATA";
    const std::string romStem = game.romPath.stem().string();
    const std::string filenameStem = Path(game.filename).stem().string();
    const std::string number = TwoDigitNumber(game.videopacNumber);

    std::vector<Path> candidates;

    auto addCandidate = [&](const std::string& stem)
    {
        if (stem.empty())
            return;
        candidates.push_back(metadataFolder / (stem + ".ini"));
        candidates.push_back(metadataFolder / (stem + ".txt"));
    };

    addCandidate(romStem);
    if (filenameStem != romStem)
        addCandidate(filenameStem);

    if (!number.empty())
    {
        addCandidate("vp_" + number);
        addCandidate(number);
    }

    for (const Path& candidate : candidates)
    {
        if (IsRegularFile(candidate))
            return candidate;
    }

    return {};
}

bool MetadataEngine::Populate(GameInfo& game) const
{
    const Path path = FindMetadata(game);
    if (path.empty())
        return false;

    std::ifstream file(path, std::ios::binary);
    if (!file)
        return false;

    std::string line;
    bool firstLine = true;

    while (std::getline(file, line))
    {
        if (firstLine)
        {
            firstLine = false;
            if (line.size() >= 3 &&
                static_cast<unsigned char>(line[0]) == 0xEF &&
                static_cast<unsigned char>(line[1]) == 0xBB &&
                static_cast<unsigned char>(line[2]) == 0xBF)
            {
                line.erase(0, 3);
            }
        }

        const std::string trimmed = Trim(line);
        if (trimmed.empty() || trimmed[0] == '#' || trimmed[0] == ';')
            continue;
        if (trimmed.front() == '[' && trimmed.back() == ']')
            continue;

        const std::size_t separator = trimmed.find('=');
        if (separator == std::string::npos)
            continue;

        ApplyValue(
            game,
            trimmed.substr(0, separator),
            trimmed.substr(separator + 1));
    }

    if (game.sortTitle.empty())
        game.sortTitle = game.title;

    return true;
}

std::size_t MetadataEngine::Populate(GameLibrary& library) const
{
    std::size_t loaded = 0;

    for (GameInfo& game : library.Games())
    {
        if (Populate(game))
            ++loaded;
    }

    return loaded;
}
