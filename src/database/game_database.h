#pragma once

#include <cstddef>
#include <filesystem>
#include <string>
#include <ctime>
#include <vector>

class GameLibrary;
struct GameInfo;

struct ProjectPage
{
    std::string pageKey;
    std::string title;
    std::string content;
    int sortOrder = 0;
};

struct GameDatabaseResult
{
    bool databaseAvailable = false;
    bool databaseCreated = false;
    bool importedGameList = false;
    std::size_t importedRecords = 0;
    std::size_t matchedGames = 0;
    std::vector<std::string> unmatchedRomFilenames;
    std::string message;
};

// Read-only identity pair for the main preservation catalogue. Used by My
// Collection to store/resolve an optional reference without ever writing to
// o2em-ng.db. rom_filename remains the stable key; the id is a convenience.
struct GameIdentity
{
    long long id = 0;
    std::string romFilename;
};

class GameDatabase
{
public:
    explicit GameDatabase(const std::filesystem::path& basePath = {});

    void SetBasePath(const std::filesystem::path& basePath);
    const std::filesystem::path& BasePath() const noexcept;
    std::filesystem::path DatabasePath() const;

    GameDatabaseResult InitializeAndPopulate(GameLibrary& library) const;
    bool SetFavorite(const std::string& romFilename, bool favorite) const;
    bool RecordLaunch(const std::string& romFilename, std::time_t when) const;

    // Saves user-owned metadata fields. These columns are deliberately kept
    // separate from imported data so Gamelist refreshes never overwrite edits.
    bool SaveUserMetadata(const GameInfo& game) const;
    bool RenameRomFilename(const std::string& oldFilename, const std::string& newFilename) const;
    bool ClearUserMetadata(const std::string& romFilename) const;
    bool DeleteGameRecord(const std::string& romFilename) const;

    // Manual "DELETE GAME DATA" support: persistently hides one exact
    // catalogue entry (by its exact ROM filename key, including any "+")
    // from catalogue seeding and from the permanent Game Library, without
    // touching ROM files or media.
    bool SuppressCatalogEntry(const std::string& romFilename) const;
    bool IsCatalogEntrySuppressed(const std::string& romFilename) const;
    std::vector<std::string> LoadSuppressedCatalogEntries() const;

    bool InitializeProjectPages() const;
    std::vector<ProjectPage> LoadProjectPages() const;
    bool SaveProjectPage(const ProjectPage& page) const;

    // READ-ONLY: returns (id, rom_filename) for every game row. My Collection
    // uses this to link to the main catalogue and to reuse its metadata/box
    // art. This never modifies o2em-ng.db.
    std::vector<GameIdentity> LoadGameIdentityMap() const;

private:
    std::filesystem::path basePath_;
};
