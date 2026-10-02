#pragma once

#include <filesystem>
#include <string>
#include <vector>

// One physical item in the user's personal Videopac collection.
//
// This struct describes ONLY user-owned collection data. It deliberately keeps
// reference_game_id / reference_rom_filename as optional pointers into the main
// game database: those fields are informational and never create a hard
// dependency on GAMEDATA/o2em-ng.db. mycollection.db must survive on its own.
struct CollectionEntry
{
    long long id = 0;
    long long referenceGameId = 0;
    std::string referenceRomFilename;
    std::string catalogId;
    std::string title;
    std::string region;

    bool owned = true;
    bool cartridge = true;
    bool box = false;
    bool manual = false;

    // Component conditions are tracked separately for the three physical
    // objects; a cartridge, a box and a manual do not share one condition.
    // All three use the shared controlled vocabulary in ConditionValues().
    std::string cartridgeCondition;
    std::string boxCondition;
    std::string manualCondition;
    std::string purchaseSource;
    std::string purchaseDate;
    std::string dateAdded;
    std::string storageLocation;
    std::string notes;

    bool wanted = false;
    int quantity = 1;

    std::string createdAt;
    std::string updatedAt;
};

// Aggregated counts for the Collection Stats panel.
struct CollectionStats
{
    int ownedCartridges = 0;
    int boxedGames = 0;
    int manuals = 0;
    int duplicates = 0;
    int wanted = 0;
    int complete = 0;
};

// Owns GAMEDATA/mycollection.db, a database completely separate from the main
// preservation database GAMEDATA/o2em-ng.db. Only collection items are ever
// written here; the main database is never modified by this class.
class CollectionDatabase
{
public:
    explicit CollectionDatabase(const std::filesystem::path& basePath = {});

    void SetBasePath(const std::filesystem::path& basePath);
    const std::filesystem::path& BasePath() const noexcept;
    std::filesystem::path DatabasePath() const;

    // Loads winsqlite3.dll, creates GAMEDATA/mycollection.db when missing and
    // ensures the collection schema exists. Returns false and fills message on
    // failure. Safe to call on every startup; it never touches o2em-ng.db.
    bool Initialize(std::string& message) const;

    // Returns every collection entry ordered by catalogue ID then title.
    std::vector<CollectionEntry> LoadEntries() const;

    // Personal-collection writes. These only ever touch mycollection.db; the
    // main preservation database is never modified.
    long long InsertEntry(const CollectionEntry& entry) const;   // new id, 0 on failure
    bool UpdateEntry(const CollectionEntry& entry) const;
    bool DeleteEntry(long long id) const;

    // Counts used by the Collection Stats panel.
    static CollectionStats ComputeStats(const std::vector<CollectionEntry>& entries);

    // Shared controlled condition vocabulary (used by Add/Edit and rendering).
    static const std::vector<std::string>& ConditionValues();
    // Shared region vocabulary.
    static const std::vector<std::string>& RegionValues();

private:
    std::filesystem::path basePath_;
};
