#pragma once

#include <SDL3/SDL.h>
#include <filesystem>
#include <string>
#include <vector>

#include "frontend_tabs.h"
#include "frontend_collection.h"
#include "frontend_chrome.h"
#include "settings.h"
#include "src/library/game_library.h"
#include "src/database/game_database.h"
#include "src/collection/collection_manager.h"
#include "src/collection/collection_database.h"
#include "src/media/asset_manager.h"
#include "src/import/import_manager.h"
#include "src/metadata/metadata_engine.h"
#include "src/update_check.h"

class FrontendApp
{
public:
    explicit FrontendApp(SDL_Window* window);
    ~FrontendApp();
    FrontendApp(const FrontendApp&) = delete;
    FrontendApp& operator=(const FrontendApp&) = delete;

    bool Initialize();
    bool HandleEvent(const SDL_Event& event);
    void Draw();
    bool IsRunning() const noexcept;
    void RequestRedraw() noexcept;

private:
    static constexpr int VisibleRows = 18;
    static constexpr Sint16 StickDeadzone = 16000;
    static constexpr int SettingsItemCount = 4;
    static constexpr int MetadataFieldCount = 15;

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    std::string settingsPath_;
    O2EMSettings settings_;
    GameLibrary library_;
    GameDatabase gameDatabase_;
    CollectionManager collections_;
    AssetManager assetManager_;
    ImportManager importManager_;
    std::string importStatus_;
    std::vector<std::string> installedBiosFiles_;
    bool c7010FirmwareInstalled_ = false;
    bool c7420FirmwareInstalled_ = false;
    MetadataEngine metadataEngine_;

    // My Collection: a fully separate page/database owned by CollectionPage.
    // Never merged into gameDatabase_/library_.
    CollectionPage collectionPage_;

    int settingsSelected_ = 0;
    int openDropdown_ = -1; // -1 none, 1 region, 3 BIOS
    FrontendTab activeTab_ = FrontendTab::Library;
    bool running_ = true;
    bool redraw_ = true;
    // Custom Win95 chrome: borderless window dragging state.
    bool windowDragActive_ = false;
    float windowDragOffsetX_ = 0.0f;
    float windowDragOffsetY_ = 0.0f;
    // Manual edge-resize state for the borderless window (bitmask:
    // 1 left, 2 right, 4 top, 8 bottom).
    int windowResizeEdges_ = 0;
    float resizeStartMouseX_ = 0.0f;
    float resizeStartMouseY_ = 0.0f;
    int resizeStartX_ = 0;
    int resizeStartY_ = 0;
    int resizeStartW_ = 0;
    int resizeStartH_ = 0;
    bool stickUpHeld_ = false;
    bool stickDownHeld_ = false;
    bool stickLeftHeld_ = false;
    bool stickRightHeld_ = false;

    bool metadataEditMode_ = false;
    bool metadataTextInput_ = false;
    int metadataSelected_ = 0;
    std::size_t metadataCaret_ = 0;
    // Game Data push buttons (EDIT GAME DATA / SAVE / CANCEL) currently held
    // down, else -1. Same Win95 pressed-state idiom as the Quick Add buttons.
    int metadataButtonPressed_ = -1;
    GameInfo metadataWorkingCopy_;
    std::string metadataManualPath_;
    int libraryDescriptionScroll_ = 0;
    int quickAddPressed_ = -1; // Library Quick Add button held down, else -1
    int folderOpenPressed_ = -1; // Library Folders OPEN button held down, else -1
    // Library Cover / Media viewer mode: box art or the relocated screenshot
    // media viewer (still images, GIF, MP4 via FrontendScreenshot_*).
    enum class LibraryMediaMode { BoxArt = 0, Screenshots };
    LibraryMediaMode libraryMediaMode_ = LibraryMediaMode::BoxArt;
    bool scaleCoverImage_ = true;
    // Favorites panel scrolling (Win95-style scrollbar; geometry shared by
    // draw and hit paths via FavoritesScrollMetrics).
    int favoritesScroll_ = 0;
    bool favoritesScrollDrag_ = false;
    float favoritesScrollDragGrab_ = 0.0f;
    std::vector<ProjectPage> projectPages_;
    int projectPageIndex_ = 0;
    int projectPageScroll_ = 0;
    // Phase 2 update check (manual only): state polled from the worker via
    // UpdateCheck's mutex-protected snapshot.
    UpdateCheck updateCheck_;
    bool updateCheckPressed_ = false;  // CHECK UPDATES button held down
    bool viewReleasePressed_ = false;  // VIEW RELEASE button held down
    UpdateCheck::State updateCheckLastSeenState_ = UpdateCheck::State::Idle;
    bool haveWindowedBounds_ = false;
    int windowedX_ = 0;
    int windowedY_ = 0;
    int windowedWidth_ = 1280;
    int windowedHeight_ = 800;

    SDL_Renderer* RefreshRenderer();
    const GameInfo* GetSelectedGame() const noexcept;
    GameInfo* GetSelectedGame() noexcept;

    void CycleCollectionView(int direction);
    void ToggleFavorite();
    void MoveSelection(int direction);
    void MoveSettingsSelection(int direction);
    void MoveMetadataSelection(int direction);
    void MoveTab(int direction);
    void SetActiveTab(FrontendTab tab);
    void ActivateSelection();
    void ActivateSettingsSelection();
    void ApplyFullscreenMode(bool enabled);
    void GoBack();

    void BeginMetadataEdit();
    void CancelMetadataEdit();
    void SaveMetadataEdit();
    void DeleteGameDataForSelectedGame();
    void ToggleMetadataTextInput();
    void EditCurrentMetadataField();
    bool IsLongMetadataField() const noexcept;
    std::string* CurrentMetadataField();
    const char* CurrentMetadataLabel() const;
    SDL_FRect MetadataButtonRect(const SDL_FRect& inner, int index) const;
    void DrawMetadataButton(const SDL_FRect& rect, int index, const char* label);
    // Shared Win95 push-button body (shadow, raised/sunken edges, centered
    // label); Game Data and Library OPEN buttons use it.
    void DrawWin95Button(const SDL_FRect& rect, const char* label, bool pressed);
    void OpenSelectedManual();
    void EditLibraryDescription();
    void EditCurrentProjectPage();
    void SelectProjectPage(int index);

    void HandleKeyDown(const SDL_KeyboardEvent& event);
    void HandleTextInput(const SDL_TextInputEvent& event);
    void HandleGamepadButtonDown(const SDL_GamepadButtonEvent& event);
    void HandleGamepadAxisMotion(const SDL_GamepadAxisEvent& event);
    void HandleMouseButtonDown(const SDL_MouseButtonEvent& event);
    void HandleMouseWheel(const SDL_MouseWheelEvent& event);

    bool TrySelectTabAt(float x, float y);
    bool TrySelectLibraryRowAt(float x, float y, bool activate);
    bool TrySelectSettingsRowAt(float x, float y, bool activate);
    bool TryActivateManualAt(float x, float y);
    bool TryMetadataControlAt(float x, float y);
    bool TryEditLibraryDescriptionAt(float x, float y);
    bool TryLibraryQuickControlAt(float x, float y);
    SDL_FRect QuickAddButtonRect(const SDL_FRect& importsPanel, int index) const;
    // Library Folders group: OPEN button rect for a row and the runtime
    // folder path each row opens (base path from AssetManager).
    SDL_FRect LibraryOpenButtonRect(const SDL_FRect& foldersPanel, int index) const;
    std::filesystem::path LibraryFolderPath(int index) const;
    SDL_FRect QuickComboRect(const SDL_FRect& inner, float y, float h) const;
    SDL_FRect LibraryCoverMediaRect(const SDL_FRect& coverPanel) const;
    void LibraryMediaButtonRects(const SDL_FRect& coverPanel,
        SDL_FRect& boxArt, SDL_FRect& screenshots) const;
    // Box Art mode's per-item DELETE (deletes only the displayed cover).
    SDL_FRect LibraryBoxArtDeleteRect(const SDL_FRect& coverPanel) const;
    bool TryLibraryMediaControlAt(float x, float y);
    void HandleMouseButtonUp(const SDL_MouseButtonEvent& event);
    void HandleMouseMotion(const SDL_MouseMotionEvent& event);
    // Favorites scrollbar: parts are 0 none, 1 up arrow, 2 down arrow,
    // 3 track page-up, 4 track page-down, 5 thumb.
    struct FavoritesScrollGeom
    {
        SDL_FRect track{};
        SDL_FRect upArrow{};
        SDL_FRect downArrow{};
        SDL_FRect thumb{};
        int maxScroll = 0;
        int visibleRows = 0;
    };
    // Clamps favoritesScroll_ against the live favorite count, so both are
    // non-const on purpose.
    FavoritesScrollGeom FavoritesScrollMetrics(const SDL_FRect& favInner,
        std::size_t favoriteCount);
    int FavoritesScrollPartAt(float x, float y, const SDL_FRect& favInner,
        std::size_t favoriteCount);
    bool HandleFavoritesScrollbarAt(float x, float y,
        const SDL_FRect& favInner, std::size_t favoriteCount);
    // Game Library list viewport scrolling: same Win95 scrollbar idiom as the
    // Favorites panel (17 px column, raised arrows, sunken track, thumb),
    // with 28 px list rows. libraryListScroll_ is the top visible row.
    int libraryListScroll_ = 0;
    bool libraryListScrollDrag_ = false;
    float libraryListScrollDragGrab_ = 0.0f;
    // Visible list rows area of the Game Library panel (below the column
    // header), shared by the draw and hit paths.
    SDL_FRect LibraryListRect(const SDL_FRect& content) const;
    FavoritesScrollGeom LibraryListScrollMetrics(const SDL_FRect& listRect,
        int itemCount);
    int LibraryListScrollPartAt(float x, float y, const SDL_FRect& listRect,
        int itemCount);
    bool HandleLibraryListScrollbarAt(float x, float y,
        const SDL_FRect& listRect, int itemCount);
    // Adjusts libraryListScroll_ so the selected game stays in view after
    // selection movement or collection/view changes; never fights wheel
    // scrolling because it only runs when the selection itself moves.
    void KeepLibrarySelectionVisible();
    bool TryLibraryFavoriteAt(float x, float y, bool activate);
    bool TryProjectControlAt(float x, float y);
    bool TryExitButtonAt(float x, float y);
    // Custom Win95 title bar / menu bar input (window drag + controls).
    bool TryChromeControlAt(float x, float y, int clicks);
    bool BeginWindowResizeAt(float x, float y, int windowWidth, int windowHeight);
    void ToggleWindowMaximize();
    bool TryImportControlAt(float x, float y);
    void RunImport(ImportAssetType type);
    void RunDelete(ImportAssetType type);
    void RefreshSelectedGameAssets(GameInfo& game);
    void RefreshInstalledBiosFiles(bool preserveSelection = true);
    bool HasInstalledBios() const noexcept;
    bool SelectedBiosExists() const noexcept;

    void DrawFrontend();
    void DrawLibraryList(const SDL_FRect& content);
    void DrawLibraryDashboard(const SDL_FRect& content);
    void DrawActiveTab(const SDL_FRect& content);
    void DrawGameInformationTab(const SDL_FRect& content);
    void DrawManualTab(const SDL_FRect& content);
    void DrawAboutTab(const SDL_FRect& content);
    void DrawCreditsTab(const SDL_FRect& content);
    void DrawProjectPage(const SDL_FRect& content);
    void DrawSettingsTab(const SDL_FRect& content);
    void DrawPlaceholderTab(const SDL_FRect& content, const char* title, const char* message);
    void DrawImportCenter(const SDL_FRect& content);
    bool TrySettingsUpdateControlAt(float x, float y);
};
