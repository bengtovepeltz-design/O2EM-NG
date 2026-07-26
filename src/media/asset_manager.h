#pragma once

#include <filesystem>

struct GameInfo;
class GameLibrary;

// Central media lookup service for the collection frontend.
//
// AssetManager owns the folder conventions used by O2EM-NG. Other modules
// receive resolved paths through GameInfo and therefore do not need to know
// where box art, cartridges, manuals or screenshots are stored.
class AssetManager
{
public:
    AssetManager() = default;
    explicit AssetManager(const std::filesystem::path& basePath);

    void SetBasePath(const std::filesystem::path& basePath);
    const std::filesystem::path& BasePath() const noexcept;

    std::filesystem::path FindBoxArt(const GameInfo& game) const;
    std::filesystem::path FindCartridge(const GameInfo& game) const;
    std::filesystem::path FindManual(const GameInfo& game) const;

    void Populate(GameInfo& game) const;
    void Populate(GameLibrary& library) const;

private:
    std::filesystem::path basePath_;
};
