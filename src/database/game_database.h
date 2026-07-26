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
    bool ClearUserMetadata(const std::string& romFilename) const;

    bool InitializeProjectPages() const;
    std::vector<ProjectPage> LoadProjectPages() const;
    bool SaveProjectPage(const ProjectPage& page) const;

private:
    std::filesystem::path basePath_;
};
