#include "game_database.h"

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../library/game_info.h"
#include "../library/game_library.h"

namespace
{
    struct sqlite3;
    struct sqlite3_stmt;
    using sqlite3_int64 = long long;
    using sqlite3_destructor_type = void(__cdecl*)(void*);

    constexpr int SQLITE_OK = 0;
    constexpr int SQLITE_ROW = 100;
    constexpr int SQLITE_DONE = 101;
    constexpr int SQLITE_OPEN_READWRITE = 0x00000002;
    constexpr int SQLITE_OPEN_CREATE = 0x00000004;
    const sqlite3_destructor_type SQLITE_TRANSIENT =
        reinterpret_cast<sqlite3_destructor_type>(-1);

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
            errMsg = Load<ErrMsg>("sqlite3_errmsg");

            available_ = openV2 && close && exec && freeMemory && prepareV2 &&
                step && finalize && bindText && bindInt && columnText &&
                columnInt && reset && clearBindings && errMsg;
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

    struct ImportedRecord
    {
        std::string filename;
        std::string title;
        std::string notes;
        std::string category;
        int plusVersion = 0;
        int prototype = 0;
        int unreleased = 0;
        int alternate = 0;
        int hack = 0;
        int fixed = 0;
    };

    std::string Trim(const std::string& value)
    {
        const auto first = std::find_if_not(value.begin(), value.end(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        if (first == value.end())
            return {};
        const auto last = std::find_if_not(value.rbegin(), value.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; }).base();
        return std::string(first, last);
    }

    std::string ToLower(std::string value)
    {
        std::transform(value.begin(), value.end(), value.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return value;
    }

    bool ContainsInsensitive(const std::string& text, const std::string& token)
    {
        return ToLower(text).find(ToLower(token)) != std::string::npos;
    }

    std::string CategoryFromFilename(const std::string& filename)
    {
        const std::string lower = ToLower(filename);
        if (lower.rfind("vp_", 0) == 0) return "Videopac";
        if (lower.rfind("o2_", 0) == 0) return "Odyssey2";
        if (lower.rfind("jo_", 0) == 0) return "Jopac";
        if (lower.rfind("pb_", 0) == 0) return "Parker Brothers";
        if (lower.rfind("im_", 0) == 0) return "Imagic";
        if (lower.rfind("pr_", 0) == 0) return "Prototype";
        if (lower.rfind("new_", 0) == 0) return "Homebrew";
        if (lower.rfind("mod_", 0) == 0) return "Modified";
        if (lower.rfind("br_", 0) == 0) return "Brazilian";
        if (lower.rfind("pal_", 0) == 0) return "PAL";
        if (lower.rfind("ntsc_", 0) == 0) return "NTSC";
        if (lower.rfind("bios_", 0) == 0) return "BIOS";
        return "Other";
    }

    std::vector<ImportedRecord> ParseGameList(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::vector<ImportedRecord> records;
        if (!file)
            return records;

        std::string line;
        while (std::getline(file, line))
        {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();

            const std::string trimmed = Trim(line);
            if (trimmed.empty() || trimmed.rfind("GAME LIST:", 0) == 0)
                continue;

            const std::size_t binPosition = ToLower(trimmed).find(".bin");
            if (binPosition == std::string::npos)
                continue;

            ImportedRecord record;
            record.filename = Trim(trimmed.substr(0, binPosition + 4));
            std::string remainder = Trim(trimmed.substr(binPosition + 4));

            // Entries without a title are still useful database records.
            record.title = remainder;
            record.notes.clear();

            const std::size_t noteStart = remainder.find("  ");
            if (noteStart != std::string::npos)
            {
                record.title = Trim(remainder.substr(0, noteStart));
                record.notes = Trim(remainder.substr(noteStart));
            }

            if (record.title.empty())
                record.title = std::filesystem::path(record.filename).stem().string();

            record.category = CategoryFromFilename(record.filename);
            const std::string flags = ToLower(record.filename + " " + remainder);
            record.plusVersion = flags.find("_pl") != std::string::npos ||
                flags.find("plus version") != std::string::npos ||
                flags.find(" +") != std::string::npos;
            record.prototype = record.category == "Prototype" ||
                flags.find("prototype") != std::string::npos ||
                flags.find(" proto") != std::string::npos;
            record.unreleased = flags.find("unreleased") != std::string::npos;
            record.alternate = flags.find("alt") != std::string::npos ||
                flags.find("alternate") != std::string::npos;
            record.hack = flags.find("hack") != std::string::npos ||
                flags.find("modified") != std::string::npos;
            record.fixed = flags.find("fix") != std::string::npos ||
                flags.find("bug fixed") != std::string::npos;

            records.push_back(std::move(record));
        }
        return records;
    }

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

    std::filesystem::path FindGameList(const std::filesystem::path& basePath)
    {
        // During development the executable normally runs from x64/Debug or
        // x64/Release while GAMEDATA lives in the project root. Search the
        // runtime folder and a few parent folders so both layouts work.
        std::filesystem::path root = basePath;
        std::error_code error;
        for (int level = 0; level < 5 && !root.empty(); ++level)
        {
            const std::filesystem::path candidates[] = {
                root / "GAMEDATA" / "Gamelist.txt",
                root / "GAMEDATA" / "GameList.txt",
                root / "Gamelist.txt",
                root / "GameList.txt",
                root / "Gamelist(2).txt"
            };

            for (const auto& candidate : candidates)
            {
                if (std::filesystem::is_regular_file(candidate, error) && !error)
                    return candidate;
                error.clear();
            }

            const std::filesystem::path parent = root.parent_path();
            if (parent == root)
                break;
            root = parent;
        }
        return {};
    }

    bool BindText(const WinSQLite& api, sqlite3_stmt* statement, int index, const std::string& text)
    {
        return api.bindText(statement, index, text.c_str(),
            static_cast<int>(text.size()), SQLITE_TRANSIENT) == SQLITE_OK;
    }

    std::string ColumnString(const WinSQLite& api, sqlite3_stmt* statement, int column)
    {
        const unsigned char* text = api.columnText(statement, column);
        return text ? reinterpret_cast<const char*>(text) : std::string{};
    }

    std::string AppendFlag(std::string text, const char* flag)
    {
        if (!text.empty())
            text += " | ";
        text += flag;
        return text;
    }
}

GameDatabase::GameDatabase(const std::filesystem::path& basePath)
{
    SetBasePath(basePath);
}

void GameDatabase::SetBasePath(const std::filesystem::path& basePath)
{
    basePath_ = basePath.empty() ? std::filesystem::current_path() : basePath;
}

const std::filesystem::path& GameDatabase::BasePath() const noexcept
{
    return basePath_;
}

std::filesystem::path GameDatabase::DatabasePath() const
{
    return basePath_ / "GAMEDATA" / "o2em-ng.db";
}

GameDatabaseResult GameDatabase::InitializeAndPopulate(GameLibrary& library) const
{
    GameDatabaseResult result;
    WinSQLite api;
    if (!api.Available())
    {
        result.message = "winsqlite3.dll could not be loaded.";
        return result;
    }
    result.databaseAvailable = true;

    std::error_code fsError;
    std::filesystem::create_directories(DatabasePath().parent_path(), fsError);

    const std::filesystem::path databasePath = DatabasePath();
    const bool existed = std::filesystem::exists(databasePath, fsError);
    const std::string databasePathString = databasePath.string();

    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
    {
        result.message = "Could not open the game database.";
        return result;
    }
    result.databaseCreated = !existed;

    std::string error;
    const char* schema =
        "PRAGMA journal_mode=WAL;"
        "CREATE TABLE IF NOT EXISTS games ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "rom_filename TEXT NOT NULL UNIQUE COLLATE NOCASE,"
        "title TEXT NOT NULL DEFAULT '',"
        "notes TEXT NOT NULL DEFAULT '',"
        "category TEXT NOT NULL DEFAULT '',"
        "plus_version INTEGER NOT NULL DEFAULT 0,"
        "prototype INTEGER NOT NULL DEFAULT 0,"
        "unreleased INTEGER NOT NULL DEFAULT 0,"
        "alternate INTEGER NOT NULL DEFAULT 0,"
        "hack INTEGER NOT NULL DEFAULT 0,"
        "fixed INTEGER NOT NULL DEFAULT 0,"
        "favorite INTEGER NOT NULL DEFAULT 0,"
        "play_count INTEGER NOT NULL DEFAULT 0,"
        "last_played INTEGER NOT NULL DEFAULT 0,"
        "user_title TEXT NOT NULL DEFAULT '',"
        "year TEXT NOT NULL DEFAULT '',"
        "publisher TEXT NOT NULL DEFAULT '',"
        "developer TEXT NOT NULL DEFAULT '',"
        "genre TEXT NOT NULL DEFAULT '',"
        "players TEXT NOT NULL DEFAULT '',"
        "controls TEXT NOT NULL DEFAULT '',"
        "voice_module TEXT NOT NULL DEFAULT '',"
        "videopac_plus_text TEXT NOT NULL DEFAULT '',"
        "rating TEXT NOT NULL DEFAULT '',"
        "short_description TEXT NOT NULL DEFAULT '',"
        "user_description TEXT NOT NULL DEFAULT '',"
        "trivia TEXT NOT NULL DEFAULT '',"
        "manual_path TEXT NOT NULL DEFAULT ''"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_games_filename ON games(rom_filename);";

    if (!Execute(api, handle.Get(), schema, error))
    {
        result.message = "Database schema error: " + error;
        return result;
    }

    const char* migrations[] = {
        "ALTER TABLE games ADD COLUMN user_title TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN year TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN publisher TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN developer TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN genre TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN players TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN controls TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN voice_module TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN videopac_plus_text TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN rating TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN short_description TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN user_description TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN trivia TEXT NOT NULL DEFAULT '';",
        "ALTER TABLE games ADD COLUMN manual_path TEXT NOT NULL DEFAULT '';"
    };
    for (const char* migration : migrations)
    {
        std::string ignored;
        Execute(api, handle.Get(), migration, ignored);
    }

    // Import or refresh the master list on every startup. UPSERT updates
    // descriptive fields while preserving local state such as favorites,
    // play count and last played.
    const std::filesystem::path gameList = FindGameList(basePath_);
    const std::vector<ImportedRecord> records = ParseGameList(gameList);

    if (!records.empty())
    {
        if (!Execute(api, handle.Get(), "BEGIN IMMEDIATE TRANSACTION;", error))
        {
            result.message = "Could not begin database import: " + error;
            return result;
        }

        Statement upsert(api, handle.Get(),
            "INSERT INTO games("
            "rom_filename,title,notes,category,plus_version,prototype,"
            "unreleased,alternate,hack,fixed) VALUES(?,?,?,?,?,?,?,?,?,?) "
            "ON CONFLICT(rom_filename) DO UPDATE SET "
            "title=excluded.title,notes=excluded.notes,category=excluded.category,"
            "plus_version=excluded.plus_version,prototype=excluded.prototype,"
            "unreleased=excluded.unreleased,alternate=excluded.alternate,"
            "hack=excluded.hack,fixed=excluded.fixed;");

        if (!upsert)
        {
            Execute(api, handle.Get(), "ROLLBACK;", error);
            result.message = "Could not prepare database import.";
            return result;
        }

        bool importFailed = false;
        for (const ImportedRecord& record : records)
        {
            BindText(api, upsert.Get(), 1, record.filename);
            BindText(api, upsert.Get(), 2, record.title);
            BindText(api, upsert.Get(), 3, record.notes);
            BindText(api, upsert.Get(), 4, record.category);
            api.bindInt(upsert.Get(), 5, record.plusVersion);
            api.bindInt(upsert.Get(), 6, record.prototype);
            api.bindInt(upsert.Get(), 7, record.unreleased);
            api.bindInt(upsert.Get(), 8, record.alternate);
            api.bindInt(upsert.Get(), 9, record.hack);
            api.bindInt(upsert.Get(), 10, record.fixed);

            if (api.step(upsert.Get()) == SQLITE_DONE)
                ++result.importedRecords;
            else
                importFailed = true;

            api.reset(upsert.Get());
            api.clearBindings(upsert.Get());
        }

        if (importFailed || !Execute(api, handle.Get(), "COMMIT;", error))
        {
            Execute(api, handle.Get(), "ROLLBACK;", error);
            result.message = "Database import failed: " + error;
            return result;
        }
        result.importedGameList = true;
    }

    // Patch 0022: Gamelist.txt is the master catalogue, while LoadRoms()
    // only creates entries for ROM files that are already installed. Add
    // missing official catalogue entries here so a clean installation has
    // selectable games and can import its first ROM. Existing ROM-backed
    // entries remain authoritative and are never duplicated.
    std::unordered_map<std::string, bool> libraryFilenames;
    for (const GameInfo& game : library.Games())
    {
        std::string key = game.filename;
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        libraryFilenames[key] = true;
    }

    for (const ImportedRecord& record : records)
    {
        const RomClassification classification =
            ClassifyRom(std::filesystem::path(record.filename));
        if (!classification.official)
            continue;

        std::string key = record.filename;
        std::transform(key.begin(), key.end(), key.begin(),
            [](unsigned char character)
            {
                return static_cast<char>(std::tolower(character));
            });
        if (libraryFilenames.find(key) != libraryFilenames.end())
            continue;

        GameInfo game;
        game.filename = record.filename;
        game.title = record.title.empty() ? record.filename : record.title;
        game.sortTitle = game.title;
        game.genre = record.category;
        game.description = record.notes;
        game.videopacNumber = ParseVideopacNumberFromFilename(record.filename);
        game.videopacPlus = record.plusVersion ? "Yes" : "No";
        game.rom.name = game.title;
        game.rom.info = classification;
        // romPath and rom.path deliberately stay empty until import succeeds.

        library.Add(game);
        libraryFilenames[key] = true;
    }

    Statement select(api, handle.Get(),
        "SELECT title,notes,category,plus_version,prototype,unreleased,"
        "alternate,hack,fixed,favorite,play_count,last_played,"
        "user_title,year,publisher,developer,genre,players,controls,"
        "voice_module,videopac_plus_text,rating,short_description,"
        "user_description,trivia,manual_path "
        "FROM games WHERE rom_filename=? COLLATE NOCASE;");

    if (!select)
    {
        result.message = "Could not prepare database lookup.";
        return result;
    }

    for (GameInfo& game : library.Games())
    {
        BindText(api, select.Get(), 1, game.filename);
        if (api.step(select.Get()) == SQLITE_ROW)
        {
            const std::string title = ColumnString(api, select.Get(), 0);
            const std::string notes = ColumnString(api, select.Get(), 1);
            const std::string category = ColumnString(api, select.Get(), 2);

            if (!title.empty())
            {
                game.title = title;
                game.sortTitle = title;
            }
            if (game.genre.empty() && !category.empty())
                game.genre = category;
            if (game.description.empty() && !notes.empty())
                game.description = notes;

            std::string flags;
            if (api.columnInt(select.Get(), 3)) flags = AppendFlag(flags, "Plus");
            if (api.columnInt(select.Get(), 4)) flags = AppendFlag(flags, "Prototype");
            if (api.columnInt(select.Get(), 5)) flags = AppendFlag(flags, "Unreleased");
            if (api.columnInt(select.Get(), 6)) flags = AppendFlag(flags, "Alternate");
            if (api.columnInt(select.Get(), 7)) flags = AppendFlag(flags, "Hack/Modified");
            if (api.columnInt(select.Get(), 8)) flags = AppendFlag(flags, "Fixed");
            if (!flags.empty())
            {
                if (!game.description.empty())
                    game.description += " | ";
                game.description += flags;
            }

            const std::string userTitle = ColumnString(api, select.Get(), 12);
            if (!userTitle.empty())
            {
                game.title = userTitle;
                game.sortTitle = userTitle;
            }
            const std::string userYear = ColumnString(api, select.Get(), 13);
            const std::string userPublisher = ColumnString(api, select.Get(), 14);
            const std::string userDeveloper = ColumnString(api, select.Get(), 15);
            const std::string userGenre = ColumnString(api, select.Get(), 16);
            const std::string userPlayers = ColumnString(api, select.Get(), 17);
            const std::string userControls = ColumnString(api, select.Get(), 18);
            const std::string userVoiceModule = ColumnString(api, select.Get(), 19);
            const std::string userVideopacPlus = ColumnString(api, select.Get(), 20);
            const std::string userRating = ColumnString(api, select.Get(), 21);
            const std::string userShortDescription = ColumnString(api, select.Get(), 22);
            const std::string userDescription = ColumnString(api, select.Get(), 23);
            const std::string userTrivia = ColumnString(api, select.Get(), 24);

            if (!userYear.empty()) game.year = userYear;
            if (!userPublisher.empty()) game.publisher = userPublisher;
            if (!userDeveloper.empty()) game.developer = userDeveloper;
            if (!userGenre.empty()) game.genre = userGenre;
            if (!userPlayers.empty()) game.players = userPlayers;
            if (!userControls.empty()) game.controls = userControls;
            if (!userVoiceModule.empty()) game.voiceModule = userVoiceModule;
            if (!userVideopacPlus.empty()) game.videopacPlus = userVideopacPlus;
            if (!userRating.empty()) game.rating = userRating;
            if (!userShortDescription.empty()) game.shortDescription = userShortDescription;
            if (!userDescription.empty()) game.description = userDescription;
            if (!userTrivia.empty()) game.trivia = userTrivia;
            const std::string manualPath = ColumnString(api, select.Get(), 25);
            if (!manualPath.empty()) game.manual = basePath_ / manualPath;

            game.favorite = api.columnInt(select.Get(), 9) != 0;
            game.playCount = api.columnInt(select.Get(), 10);
            game.lastPlayed = static_cast<std::time_t>(api.columnInt(select.Get(), 11));
            ++result.matchedGames;
        }
        else
        {
            result.unmatchedRomFilenames.push_back(game.filename);
        }
        api.reset(select.Get());
        api.clearBindings(select.Get());
    }

    std::ostringstream message;
    message << "Database ready: " << result.matchedGames << " of "
            << library.Count() << " library games matched";
    if (result.importedGameList)
        message << ", " << result.importedRecords << " records refreshed";
    else
        message << ", no Gamelist.txt found";
    message << ". DB: " << DatabasePath().string();
    result.message = message.str();
    return result;
}


bool GameDatabase::SetFavorite(const std::string& romFilename, bool favorite) const
{
    WinSQLite api;
    if (!api.Available())
        return false;

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return false;

    Statement statement(api, handle.Get(),
        "UPDATE games SET favorite=? WHERE rom_filename=? COLLATE NOCASE;");
    if (!statement)
        return false;

    api.bindInt(statement.Get(), 1, favorite ? 1 : 0);
    BindText(api, statement.Get(), 2, romFilename);
    return api.step(statement.Get()) == SQLITE_DONE;
}

bool GameDatabase::RecordLaunch(const std::string& romFilename, std::time_t when) const
{
    WinSQLite api;
    if (!api.Available())
        return false;

    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK)
        return false;

    Statement statement(api, handle.Get(),
        "UPDATE games SET play_count=play_count+1,last_played=? "
        "WHERE rom_filename=? COLLATE NOCASE;");
    if (!statement)
        return false;

    api.bindInt(statement.Get(), 1, static_cast<int>(when));
    BindText(api, statement.Get(), 2, romFilename);
    return api.step(statement.Get()) == SQLITE_DONE;
}


bool GameDatabase::SaveUserMetadata(const GameInfo& game) const
{
    WinSQLite api;
    if (!api.Available()) return false;
    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) return false;
    Statement statement(api, handle.Get(),
        "UPDATE games SET user_title=?,year=?,publisher=?,developer=?,genre=?,"
        "players=?,controls=?,voice_module=?,videopac_plus_text=?,rating=?,"
        "short_description=?,user_description=?,trivia=?,manual_path=? "
        "WHERE rom_filename=? COLLATE NOCASE;");
    if (!statement) return false;
    std::string manualPath;
    if (!game.manual.empty())
    {
        std::error_code error;
        const std::filesystem::path relative = std::filesystem::relative(game.manual, basePath_, error);
        manualPath = error ? game.manual.string() : relative.string();
    }
    const std::string values[] = {
        game.title, game.year, game.publisher, game.developer, game.genre,
        game.players, game.controls, game.voiceModule, game.videopacPlus,
        game.rating, game.shortDescription, game.description, game.trivia,
        manualPath, game.filename
    };
    for (int index = 0; index < 15; ++index)
        if (!BindText(api, statement.Get(), index + 1, values[index])) return false;
    return api.step(statement.Get()) == SQLITE_DONE;
}

bool GameDatabase::ClearUserMetadata(const std::string& romFilename) const
{
    WinSQLite api;
    if (!api.Available()) return false;
    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) return false;
    Statement statement(api, handle.Get(),
        "UPDATE games SET user_title='',year='',publisher='',developer='',genre='',"
        "players='',controls='',voice_module='',videopac_plus_text='',rating='',"
        "short_description='',user_description='',trivia='',manual_path='' "
        "WHERE rom_filename=? COLLATE NOCASE;");
    if (!statement) return false;
    BindText(api, statement.Get(), 1, romFilename);
    return api.step(statement.Get()) == SQLITE_DONE;
}


bool GameDatabase::InitializeProjectPages() const
{
    WinSQLite api;
    if (!api.Available()) return false;
    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) return false;

    std::string error;
    const char* schema =
        "CREATE TABLE IF NOT EXISTS project_pages ("
        "page_key TEXT PRIMARY KEY COLLATE NOCASE,"
        "title TEXT NOT NULL DEFAULT '',"
        "content TEXT NOT NULL DEFAULT '',"
        "sort_order INTEGER NOT NULL DEFAULT 0,"
        "updated_at TEXT NOT NULL DEFAULT (datetime('now'))"
        ");";
    if (!Execute(api, handle.Get(), schema, error)) return false;

    struct DefaultPage { const char* key; const char* title; const char* content; int order; };
    const DefaultPage defaults[] = {
        {"about", "About", "O2EM-NG is a modern Windows continuation of the original O2EM emulator.\n\nMission\nPreserve the original emulation core while modernizing the surrounding Windows experience.\n\nProject principles\nCompatibility before features. Accuracy before speed. Preservation before convenience.\n\nPlatform\nWindows x64, C/C++, SDL3, SQLite and PDFium.", 0},
        {"credits", "Credits", "ORIGINAL O2EM\n\nOriginal developers and contributors:\nAdd verified names and roles here.\n\nO2EM-NG\n\nProject direction and Windows modernization:\nBengt-Ove Peltz\n\nTHIRD-PARTY TECHNOLOGY\n\nSDL3\nSQLite\nPDFium\nMicrosoft Visual Studio and Windows SDK", 1},
        {"contributors", "Contributors", "Add people who have contributed code, testing, documentation, metadata, scans, research or other project work here.\n\nSuggested format:\nName\nRole or contribution\nOptional notes", 2},
        {"special_thanks", "Special Thanks", "Videopac and Odyssey2 communities\nCollectors and archivists\nBeta testers\nEveryone helping preserve the platform for future generations", 3},
        {"roadmap", "Roadmap", "BETA 3\nAbout and project information system\nCredits and contributors\nShelf View status review\nFrontend polish\nRegression testing\nDocumentation and release package\n\nFUTURE\nAdd planned features and priorities here.", 4},
        {"release_notes", "Release Notes", "0.24.0-beta - Beta 3\n\nAdd the final Beta 3 release notes here before packaging.\n\nOlder release history remains available in Docs/CHANGELOG.md.", 5}
    };
    Statement insert(api, handle.Get(),
        "INSERT OR IGNORE INTO project_pages(page_key,title,content,sort_order) VALUES(?,?,?,?);");
    if (!insert) return false;
    for (const DefaultPage& page : defaults)
    {
        BindText(api, insert.Get(), 1, page.key);
        BindText(api, insert.Get(), 2, page.title);
        BindText(api, insert.Get(), 3, page.content);
        api.bindInt(insert.Get(), 4, page.order);
        if (api.step(insert.Get()) != SQLITE_DONE) return false;
        api.reset(insert.Get());
        api.clearBindings(insert.Get());
    }
    return true;
}

std::vector<ProjectPage> GameDatabase::LoadProjectPages() const
{
    std::vector<ProjectPage> pages;
    WinSQLite api;
    if (!api.Available()) return pages;
    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) return pages;
    Statement statement(api, handle.Get(),
        "SELECT page_key,title,content,sort_order FROM project_pages ORDER BY sort_order,page_key;");
    if (!statement) return pages;
    while (api.step(statement.Get()) == SQLITE_ROW)
    {
        ProjectPage page;
        const unsigned char* key = api.columnText(statement.Get(), 0);
        const unsigned char* title = api.columnText(statement.Get(), 1);
        const unsigned char* content = api.columnText(statement.Get(), 2);
        page.pageKey = key ? reinterpret_cast<const char*>(key) : "";
        page.title = title ? reinterpret_cast<const char*>(title) : "";
        page.content = content ? reinterpret_cast<const char*>(content) : "";
        page.sortOrder = api.columnInt(statement.Get(), 3);
        pages.push_back(std::move(page));
    }
    return pages;
}

bool GameDatabase::SaveProjectPage(const ProjectPage& page) const
{
    WinSQLite api;
    if (!api.Available()) return false;
    const std::string databasePathString = DatabasePath().string();
    DatabaseHandle handle(api);
    if (api.openV2(databasePathString.c_str(), handle.Address(),
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE, nullptr) != SQLITE_OK) return false;
    Statement statement(api, handle.Get(),
        "UPDATE project_pages SET title=?,content=?,sort_order=?,updated_at=datetime('now') "
        "WHERE page_key=? COLLATE NOCASE;");
    if (!statement) return false;
    BindText(api, statement.Get(), 1, page.title);
    BindText(api, statement.Get(), 2, page.content);
    api.bindInt(statement.Get(), 3, page.sortOrder);
    BindText(api, statement.Get(), 4, page.pageKey);
    return api.step(statement.Get()) == SQLITE_DONE;
}
