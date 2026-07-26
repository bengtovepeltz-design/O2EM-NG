#pragma once

#include <filesystem>
#include <string>

struct GameInfo;

enum class ImportAssetType
{
    Rom,
    Bios,
    Manual,
    Cover,
    Screenshot
};

struct ImportResult
{
    bool success = false;
    bool cancelled = false;
    std::filesystem::path destination;
    std::string message;
};

// Central import service for Beta 3. It owns file selection, destination
// folders, stable naming and safe copying so the frontend never asks users to
// browse the O2EM-NG installation folders manually.
class ImportManager
{
public:
    ImportManager() = default;
    explicit ImportManager(const std::filesystem::path& basePath);

    void SetBasePath(const std::filesystem::path& basePath);
    const std::filesystem::path& BasePath() const noexcept;

    ImportResult ImportForGame(ImportAssetType type, const GameInfo& game) const;
    ImportResult ImportBios() const;

    ImportResult DeleteForGame(ImportAssetType type, const GameInfo& game,
        const std::filesystem::path& screenshotPath = {}) const;
    ImportResult DeleteBios() const;

private:
    std::filesystem::path basePath_;
};
