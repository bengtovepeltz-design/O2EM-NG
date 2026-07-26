#pragma once

#include <cstddef>
#include <filesystem>

struct GameInfo;
class GameLibrary;

// Loads optional collection metadata from the METADATA folder.
//
// Metadata files are simple UTF-8 key=value text files. The preferred file
// name is the ROM stem (for example "vp_01.ini"), with Videopac-number based
// fallbacks supported for official releases.
class MetadataEngine
{
public:
    MetadataEngine() = default;
    explicit MetadataEngine(const std::filesystem::path& basePath);

    void SetBasePath(const std::filesystem::path& basePath);
    const std::filesystem::path& BasePath() const noexcept;

    std::filesystem::path FindMetadata(const GameInfo& game) const;
    bool Populate(GameInfo& game) const;
    std::size_t Populate(GameLibrary& library) const;

private:
    std::filesystem::path basePath_;
};
