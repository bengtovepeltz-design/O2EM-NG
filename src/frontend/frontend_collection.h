#pragma once

#include <SDL3/SDL.h>

#include <cstddef>
#include <filesystem>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include "src/collection/collection_database.h"

struct GameInfo;
class GameLibrary;
class GameDatabase;

// My Collection: dedicated state and UI for the personal PHYSICAL Videopac
// collection page plus the compact Library front-page summary panel.
//
// All collection-specific rendering, input, search, sorting, dialogs and CSV
// export live here so FrontendApp stays a coordinator. Nothing in this module
// ever writes to the main preservation database (GAMEDATA/o2em-ng.db); it only
// reads it (title/catalog/cover) to resolve references.
class CollectionPage
{
public:
    CollectionPage();

    // basePath is the folder containing GAMEDATA/. library/gameDatabase are
    // read-only references used for metadata and cover reuse.
    void Initialize(const std::filesystem::path& basePath,
                    const GameLibrary* library,
                    const GameDatabase* gameDatabase,
                    SDL_Window* window);
    // Rebuilds the read-only main-library reference maps (call after the main
    // library has been fully populated).
    void RefreshReferences();

    // Reloads mycollection.db and recomputes the search/sort view.
    void Refresh();

    // Draws the full My Collection page into `content` (right frontend panel).
    void Draw(SDL_Renderer* renderer, const SDL_FRect& content);

    // Compact Library front-page overview panel (drawn into the `panel` rect).
    void DrawLibrarySummary(SDL_Renderer* renderer, const SDL_FRect& panel) const;
    bool HandleLibrarySummaryWheel(const SDL_FRect& panel,
        float x, float y, float deltaY);

    // Input. Coordinates are window pixels. Returns true when consumed.
    bool HandleMouseDown(float x, float y, int clicks);
    bool HandleMouseUp(float x, float y);
    bool HandleMouseWheel(float x, float y, float deltaY);
    bool HandleKeyDown(const SDL_KeyboardEvent& event);
    bool HandleTextInput(const SDL_TextInputEvent& event);

    // Controller helpers.
    void MoveSelection(int direction);
    void ActivateSelected();
    bool CancelOrBack();   // true when it handled back (e.g. closed a dialog)

    std::string StatusText(const GameLibrary& library) const;

    bool IsDialogOpen() const noexcept { return dialogOpen_; }

    // Called when leaving the tab: closes any dialog and stops text input.
    void Deactivate(SDL_Window* window);

private:
    struct CatalogKey
    {
        int group = 2;   // 0 numbered, 1 hardware C..., 2 none
        int number = 0;
        int variant = 0;
        std::string text;
    };

    CatalogKey MakeCatalogKey(const std::string& catalogId) const;
    bool MatchesSearch(const CollectionEntry& entry) const;
    void RebuildView();
    void ClampSelection();
    std::string ResolvedTitle(const CollectionEntry& entry) const;
    const GameInfo* FindReferenceGame(const CollectionEntry& entry) const;

    int SelectedEntryIndex() const;          // index into entries_, or -1
    CollectionEntry* SelectedEntry();
    void SelectEntryById(long long id);
    void SetStatus(std::string text);

    void OpenAddDialog();
    void OpenEditDialog();
    void SaveDialog();
    void CloseDialog(SDL_Window* window);
    void ToggleWanted();
    void RemoveSelected();
    void ExportCsv();

    void DrawDialog(SDL_Renderer* renderer, const SDL_FRect& content);
    bool HandleDialogMouseDown(const SDL_FRect& content, float x, float y);
    bool HandleDialogMouseUp(const SDL_FRect& content, float x, float y);
    bool HandleDialogKeyDown(const SDL_KeyboardEvent& event);
    bool HandleDialogTextInput(const SDL_TextInputEvent& event);
    std::string* DialogFieldString(int field);

    SDL_FRect ContentRect() const;

    // ---- persistent state -------------------------------------------------
    CollectionDatabase database_;
    std::filesystem::path basePath_;
    const GameLibrary* library_ = nullptr;
    const GameDatabase* gameDatabase_ = nullptr;
    SDL_Window* window_ = nullptr;

    std::vector<CollectionEntry> entries_;
    std::vector<int> view_;                  // indices into entries_
    std::unordered_map<std::string, long long> gameIdByFilename_;
    std::unordered_map<long long, std::string> filenameByGameId_;
    CollectionStats stats_;

    int selected_ = -1;                       // index into view_
    int scroll_ = 0;
    long long preserveSelectedId_ = 0;        // selection kept across a reload
    std::string search_;
    bool searchFocused_ = false;
    std::size_t searchCaret_ = 0;
    int sortColumn_ = 1;                      // 1 = No.
    bool sortAscending_ = true;
    std::string status_;
    int buttonPressed_ = -1;

    // ---- Add/Edit dialog --------------------------------------------------
    bool dialogOpen_ = false;
    bool dialogEditing_ = false;
    long long dialogEntryId_ = 0;
    CollectionEntry dialogEntry_;
    int dialogFocus_ = 0;
    std::size_t dialogCaret_ = 0;
    // Minimal text selection for clipboard behavior (Ctrl+A/C/X/V) in every
    // editable collection field. Anchor is the fixed end; caret is the moving
    // end. No document-wide editor framework is introduced.
    std::size_t dialogSelAnchor_ = 0;
    bool dialogSelActive_ = false;
    bool dialogTextActive_ = false;
    int dialogNotesScroll_ = 0;               // first visible Notes line
    int dialogComboOpen_ = -1;
    int dialogButtonPressed_ = -1;
    std::string dialogQuantityText_ = "1";
    std::vector<int> refOrder_;               // library indices, catalogue order
    int dialogRefSelected_ = -1;              // index into refOrder_, -1 = manual
    int dialogRefScroll_ = 0;
    bool dialogRefTouched_ = false;           // user deliberately changed linkage

    // ---- Library summary --------------------------------------------------
    mutable int summaryScroll_ = 0;
};
