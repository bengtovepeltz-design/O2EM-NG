#include "collection_database.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <ctime>
#include <string>

namespace
{
    struct sqlite3;
    struct sqlite3_stmt;
    using sqlite3_destructor_type = void(__cdecl*)(void*);

    constexpr int SQLITE_OK = 0;
    constexpr int SQLITE_ROW = 100;
    constexpr int SQLITE_DONE = 101;
    constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
    constexpr int SQLITE_OPEN_CREATE = 0x00000004;
    const sqlite3_destructor_type SQLITE_TRANSIENT =
        reinterpret_cast<sqlite3_destructor_type>(-1);

    // Minimal winsqlite3.dll binding. Deliberately self-contained so this
    // module never has to touch (or depend on) the main game database loader.
    class WinSQLite
    {
    public:
        using OpenV2 = int(__cdecl*)(const char*, sqlite3**, int, const char*);
        using Close = int(__cdecl*)(sqlite3*);
        using Exec = int(__cdecl*)(sqlite3*, const char*, int(__cdecl*)(void*, int, char**, char**), void*, char**);
        using Free = void(__cdecl*)(void*);
        using PrepareV2 = int(__cdecl*)(sqlite3*, const char*, int, sqlite3_stmt**, const char**);
        using Step = int(__cdecl*)(sqlite3_stmt*);
        using Finalize = int(__cdecl*)(sqlite3_stmt*);
        using BindText = int(__cdecl*)(sqlite3_stmt*, int, const char*, int, sqlite3_destructor_type);
        using BindInt = int(__cdecl*)(sqlite3_stmt*, int, int);
        using ColumnText = const unsigned char*(__cdecl*)(sqlite3_stmt*, int);
        using ColumnInt = int(__cdecl*)(sqlite3_stmt*, int);
        using Reset = int(__cdecl*)(sqlite3_stmt*);
        using ClearBindings = int(__cdecl*)(sqlite3_stmt*);
        using LastInsertRowId = long long(__cdecl*)(sqlite3*);
        using ErrMsg = const char*(__cdecl*)(sqlite3*);

        WinSQLite()
        {
            module_ = LoadLibraryW(L"winsqlite3.dll");
            if (!module_)
                return;

            openV2 = Load<OpenV2>("sqlite3_open_v2");
            close = Load<Close>("sqlite3_close");
            exec = Load<Exec>("sqlite3_exec");
            freeMemory = Load<Free>("sqlite3_free");
            prepareV2 = Load<PrepareV2>("sqlite3_prepare_v2");
            step = Load<Step>("sqlite3_step");
            finalize = Load<Finalize>("sqlite3_finalize");
            bindText = Load<BindText>("sqlite3_bind_text");
            bindInt = Load<BindInt>("sqlite3_bind_int");
            columnText = Load<ColumnText>("sqlite3_column_text");
            columnInt = Load<ColumnInt>("sqlite3_column_int");
            reset = Load<Reset>("sqlite3_reset");
            clearBindings = Load<ClearBindings>("sqlite3_clear_bindings");
            lastInsertRowId = Load<LastInsertRowId>("sqlite3_last_insert_rowid");
            errMsg = Load<ErrMsg>("sqlite3_errmsg");

            available_ = openV2 && close && exec && freeMemory && prepareV2 &&
                step && finalize && bindText && bindInt && columnText && columnInt &&
                reset && clearBindings && lastInsertRowId && errMsg;
        }

        ~WinSQLite()
        {
            if (module_)
                FreeLibrary(module_);
        }

        WinSQLite(const WinSQLite&) = delete;
        WinSQLite& operator=(const WinSQLite&) = delete;

        bool Available() const noexcept { return available_; }

        OpenV2 openV2 = nullptr;
        Close close = nullptr;
        Exec exec = nullptr;
        Free freeMemory = nullptr;
        PrepareV2 prepareV2 = nullptr;
        Step step = nullptr;
        Finalize finalize = nullptr;
        BindText bindText = nullptr;
        BindInt bindInt = nullptr;
        ColumnText columnText = nullptr;
        ColumnInt columnInt = nullptr;
        Reset reset = nullptr;
        ClearBindings clearBindings = nullptr;
        LastInsertRowId lastInsertRowId = nullptr;
        ErrMsg errMsg = nullptr;

    private:
        template<typename T>
        T Load(const char* name)
        {
            return reinterpret_cast<T>(GetProcAddress(module_, name));
        }

        HMODULE module_ = nullptr;
        bool available_ = false;
    };

    class DatabaseHandle
    {
    public:
        explicit DatabaseHandle(const WinSQLite& api) : api_(api) {}
        ~DatabaseHandle() { if (db_) api_.close(db_); }
        sqlite3** Address() noexcept { return &db_; }
        sqlite3* Get() const noexcept { return db_; }
    private:
        const WinSQLite& api_;
        sqlite3* db_ = nullptr;
    };

    class Statement
    {
    public:
        Statement(const WinSQLite& api, sqlite3* db, const char* sql)
            : api_(api)
        {
            if (api_.prepareV2(db, sql, -1, &statement_, nullptr) != SQLITE_OK)
                statement_ = nullptr;
        }
        ~Statement() { if (statement_) api_.finalize(statement_); }
        sqlite3_stmt* Get() const noexcept { return statement_; }
        explicit operator bool() const noexcept { return statement_ != nullptr; }
    private:
        const WinSQLite& api_;
        sqlite3_stmt* statement_ = nullptr;
    };

    bool Execute(const WinSQLite& api, sqlite3* db, const char* sql, std::string& error)
    {
        char* message = nullptr;
        const int result = api.exec(db, sql, nullptr, nullptr, &message);
        if (result == SQLITE_OK)
            return true;

        error = message ? message : api.errMsg(db);
        if (message)
            api.freeMemory(message);
        return false;
    }

    std::string ColumnString(const WinSQLite& api, sqlite3_stmt* statement, int column)
    {
        const unsigned char* text = api.columnText(statement, column);
        return text ? reinterpret_cast<const char*>(text) : std::string{};
    }

    // The physical-collection schema. Designed for Phase B CRUD: reference
    // identity is optional, ownership flags are integers, and timestamps are
    // plain TEXT so no foreign key or o2em-ng.db dependency is introduced.
    const char* kCollectionSchema =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS collection_items ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "reference_game_id INTEGER NULL,"
        "reference_rom_filename TEXT NULL,"
        "catalog_id TEXT NOT NULL DEFAULT '',"
        "title TEXT NOT NULL DEFAULT '',"
        "region TEXT NOT NULL DEFAULT '',"
        "owned INTEGER NOT NULL DEFAULT 1,"
        "cartridge INTEGER NOT NULL DEFAULT 1,"
        "box INTEGER NOT NULL DEFAULT 0,"
        "manual INTEGER NOT NULL DEFAULT 0,"
        "cartridge_condition TEXT NOT NULL DEFAULT '',"
        "box_condition TEXT NOT NULL DEFAULT '',"
        "manual_condition TEXT NOT NULL DEFAULT '',"
        "purchase_source TEXT NOT NULL DEFAULT '',"
        "purchase_date TEXT NOT NULL DEFAULT '',"
        "date_added TEXT NOT NULL DEFAULT '',"
        "storage_location TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "wanted INTEGER NOT NULL DEFAULT 0,"
        "quantity INTEGER NOT NULL DEFAULT 1,"
        "created_at TEXT NOT NULL DEFAULT '',"
        "updated_at TEXT NOT NULL DEFAULT ''"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_collection_catalog "
        "ON collection_items(catalog_id);";
}

CollectionDatabase::CollectionDatabase(const std::filesystem::path& basePath)
{
    SetBasePath(basePath);
}

void CollectionDatabase::SetBasePath(const std::filesystem::path& basePath)
{
    basePath_ = basePath.empty() ? std::filesystem::current_path() : basePath;
}

const std::filesystem::path& CollectionDatabase::BasePath() const noexcept
{
    return basePath_;
}

std::filesystem::path CollectionDatabase::DatabasePath() const
{
    return basePath_ / "GAMEDATA" / "mycollection.db";
}

bool CollectionDatabase::Initialize(std::string& message) const
{
    WinSQLite api;
    if (!api.Available())
    {
        message = "mycollection.db: winsqlite3.dll could not be loaded.";
        return false;
    }

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::string databasePathString = DatabasePath().string();
    const bool existed = std::filesystem::exists(DatabasePath(), fsError);

    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        message = "mycollection.db: could not open " + databasePathString;
        return false;
    }

    std::string error;
    if (!Execute(api, handle.Get(), kCollectionSchema, error))
    {
        message = "mycollection.db: schema error: " + error;
        return false;
    }

    // Backward-compatible migration of older databases that still have the
    // single ambiguous "condition" column: add the three component-condition
    // columns and seed cartridge_condition from the old value (cartridge is
    // the safest default). box/manual are left blank - never invented.
    // ALTER TABLE fails harmlessly when a column already exists, so the copy
    // only runs on the upgrade that actually adds cartridge_condition.
    {
        std::string ignored;
        const bool addedCartridge = Execute(api, handle.Get(),
            "ALTER TABLE collection_items ADD COLUMN cartridge_condition TEXT NOT NULL DEFAULT '';",
            ignored);
        Execute(api, handle.Get(),
            "ALTER TABLE collection_items ADD COLUMN box_condition TEXT NOT NULL DEFAULT '';",
            ignored);
        Execute(api, handle.Get(),
            "ALTER TABLE collection_items ADD COLUMN manual_condition TEXT NOT NULL DEFAULT '';",
            ignored);
        if (addedCartridge)
        {
            Execute(api, handle.Get(),
                "UPDATE collection_items SET cartridge_condition=condition "
                "WHERE condition<>'' AND cartridge_condition='';",
                ignored);
        }
    }

    message = existed
        ? "mycollection.db: collection database ready."
        : "mycollection.db: collection database created.";
    return true;
}

std::vector<CollectionEntry> CollectionDatabase::LoadEntries() const
{
    std::vector<CollectionEntry> entries;

    WinSQLite api;
    if (!api.Available())
        return entries;

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        return entries;
    }

    std::string error;
    if (!Execute(api, handle.Get(), kCollectionSchema, error))
        return entries;

    Statement statement(api, handle.Get(),
        "SELECT id,reference_game_id,reference_rom_filename,catalog_id,title,region,"
        "owned,cartridge,box,manual,cartridge_condition,box_condition,manual_condition,"
        "purchase_source,purchase_date,date_added,storage_location,notes,wanted,quantity,"
        "created_at,updated_at "
        "FROM collection_items "
        "ORDER BY catalog_id COLLATE NOCASE, title COLLATE NOCASE, id;");
    if (!statement)
        return entries;

    while (api.step(statement.Get()) == SQLITE_ROW)
    {
        sqlite3_stmt* row = statement.Get();
        CollectionEntry entry;
        entry.id = api.columnInt(row, 0);
        entry.referenceGameId = api.columnInt(row, 1);
        entry.referenceRomFilename = ColumnString(api, row, 2);
        entry.catalogId = ColumnString(api, row, 3);
        entry.title = ColumnString(api, row, 4);
        entry.region = ColumnString(api, row, 5);
        entry.owned = api.columnInt(row, 6) != 0;
        entry.cartridge = api.columnInt(row, 7) != 0;
        entry.box = api.columnInt(row, 8) != 0;
        entry.manual = api.columnInt(row, 9) != 0;
        entry.cartridgeCondition = ColumnString(api, row, 10);
        entry.boxCondition = ColumnString(api, row, 11);
        entry.manualCondition = ColumnString(api, row, 12);
        entry.purchaseSource = ColumnString(api, row, 13);
        entry.purchaseDate = ColumnString(api, row, 14);
        entry.dateAdded = ColumnString(api, row, 15);
        entry.storageLocation = ColumnString(api, row, 16);
        entry.notes = ColumnString(api, row, 17);
        entry.wanted = api.columnInt(row, 18) != 0;
        entry.quantity = api.columnInt(row, 19);
        entry.createdAt = ColumnString(api, row, 20);
        entry.updatedAt = ColumnString(api, row, 21);

        if (entry.quantity < 0)
            entry.quantity = 0;

        entries.push_back(std::move(entry));
    }

    return entries;
}

CollectionStats CollectionDatabase::ComputeStats(
    const std::vector<CollectionEntry>& entries)
{
    CollectionStats stats;
    for (const CollectionEntry& entry : entries)
    {
        if (entry.owned && entry.cartridge)
            ++stats.ownedCartridges;
        if (entry.owned && entry.box)
            ++stats.boxedGames;
        if (entry.owned && entry.manual)
            ++stats.manuals;
        if (entry.owned && entry.cartridge && entry.box && entry.manual)
            ++stats.complete;
        // Quantity model: an owned physical item has quantity >= 1; extra
        // copies beyond the first count as duplicates. A wanted-only
        // placeholder has owned = 0 and quantity = 0, so it never contributes.
        if (entry.owned && entry.quantity > 1)
            stats.duplicates += entry.quantity - 1;
        // "Wanted" counts the explicit flag (an owned entry may be wanted as
        // "want another copy"); unmarking always changes the statistic.
        if (entry.wanted)
            ++stats.wanted;
    }
    return stats;
}

const std::vector<std::string>& CollectionDatabase::ConditionValues()
{
    static const std::vector<std::string> values = {
        "", "Mint", "Near Mint", "Excellent", "Very Good",
        "Good", "Fair", "Poor", "Damaged"
    };
    return values;
}

const std::vector<std::string>& CollectionDatabase::RegionValues()
{
    static const std::vector<std::string> values = {
        "", "Europe (PAL)", "USA (NTSC)", "Brazil (PAL-M)",
        "Japan (NTSC)", "Other"
    };
    return values;
}

namespace
{
    std::string NowTimestamp()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char buffer[32]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &local);
        return buffer;
    }

    std::string TodayDate()
    {
        const std::time_t now = std::time(nullptr);
        std::tm local{};
        localtime_s(&local, &now);
        char buffer[16]{};
        std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &local);
        return buffer;
    }

    bool BindEntry(const WinSQLite& api, sqlite3_stmt* statement,
        const CollectionEntry& entry, bool includeCreatedAt)
    {
        int index = 1;
        bool ok = true;
        ok = ok && api.bindInt(statement, index++, static_cast<int>(entry.referenceGameId)) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.referenceRomFilename.c_str(),
            static_cast<int>(entry.referenceRomFilename.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.catalogId.c_str(),
            static_cast<int>(entry.catalogId.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.title.c_str(),
            static_cast<int>(entry.title.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.region.c_str(),
            static_cast<int>(entry.region.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.owned ? 1 : 0) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.cartridge ? 1 : 0) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.box ? 1 : 0) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.manual ? 1 : 0) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.cartridgeCondition.c_str(),
            static_cast<int>(entry.cartridgeCondition.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.boxCondition.c_str(),
            static_cast<int>(entry.boxCondition.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.manualCondition.c_str(),
            static_cast<int>(entry.manualCondition.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.purchaseSource.c_str(),
            static_cast<int>(entry.purchaseSource.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.purchaseDate.c_str(),
            static_cast<int>(entry.purchaseDate.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.dateAdded.c_str(),
            static_cast<int>(entry.dateAdded.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.storageLocation.c_str(),
            static_cast<int>(entry.storageLocation.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindText(statement, index++, entry.notes.c_str(),
            static_cast<int>(entry.notes.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.wanted ? 1 : 0) == SQLITE_OK;
        ok = ok && api.bindInt(statement, index++, entry.quantity) == SQLITE_OK;
        if (includeCreatedAt)
        {
            const std::string created = entry.createdAt.empty()
                ? NowTimestamp() : entry.createdAt;
            ok = ok && api.bindText(statement, index++, created.c_str(),
                static_cast<int>(created.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        }
        const std::string updated = NowTimestamp();
        ok = ok && api.bindText(statement, index++, updated.c_str(),
            static_cast<int>(updated.size()), SQLITE_TRANSIENT) == SQLITE_OK;
        return ok;
    }
}

long long CollectionDatabase::InsertEntry(const CollectionEntry& entry) const
{
    WinSQLite api;
    if (!api.Available())
        return 0;

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return 0;

    std::string error;
    if (!Execute(api, handle.Get(), kCollectionSchema, error))
        return 0;

    CollectionEntry normalized = entry;
    if (normalized.dateAdded.empty())
        normalized.dateAdded = TodayDate();

    Statement statement(api, handle.Get(),
        "INSERT INTO collection_items("
        "reference_game_id,reference_rom_filename,catalog_id,title,region,"
        "owned,cartridge,box,manual,cartridge_condition,box_condition,manual_condition,"
        "purchase_source,purchase_date,date_added,storage_location,notes,wanted,quantity,"
        "created_at,updated_at) "
        "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?);");
    if (!statement)
        return 0;
    if (!BindEntry(api, statement.Get(), normalized, true))
        return 0;
    if (api.step(statement.Get()) != SQLITE_DONE)
        return 0;
    return static_cast<long long>(api.lastInsertRowId(handle.Get()));
}

bool CollectionDatabase::UpdateEntry(const CollectionEntry& entry) const
{
    if (entry.id <= 0)
        return false;

    WinSQLite api;
    if (!api.Available())
        return false;

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return false;

    std::string error;
    if (!Execute(api, handle.Get(), kCollectionSchema, error))
        return false;

    CollectionEntry normalized = entry;
    if (normalized.dateAdded.empty())
        normalized.dateAdded = TodayDate();

    Statement statement(api, handle.Get(),
        "UPDATE collection_items SET "
        "reference_game_id=?,reference_rom_filename=?,catalog_id=?,title=?,region=?,"
        "owned=?,cartridge=?,box=?,manual=?,cartridge_condition=?,box_condition=?,"
        "manual_condition=?,purchase_source=?,purchase_date=?,date_added=?,"
        "storage_location=?,notes=?,wanted=?,quantity=?,updated_at=? WHERE id=?;");
    if (!statement)
        return false;
    // BindEntry performs 20 binds when includeCreatedAt=false (19 fields plus
    // updated_at); the WHERE id is bound last.
    if (!BindEntry(api, statement.Get(), normalized, false))
        return false;
    if (api.bindInt(statement.Get(), 21, static_cast<int>(normalized.id)) != SQLITE_OK)
        return false;
    return api.step(statement.Get()) == SQLITE_DONE;
}

bool CollectionDatabase::DeleteEntry(long long id) const
{
    if (id <= 0)
        return false;

    WinSQLite api;
    if (!api.Available())
        return false;

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return false;

    std::string error;
    if (!Execute(api, handle.Get(), kCollectionSchema, error))
        return false;

    Statement statement(api, handle.Get(),
        "DELETE FROM collection_items WHERE id=?;");
    if (!statement)
        return false;
    if (api.bindInt(statement.Get(), 1, static_cast<int>(id)) != SQLITE_OK)
        return false;
    return api.step(statement.Get()) == SQLITE_DONE;
}
