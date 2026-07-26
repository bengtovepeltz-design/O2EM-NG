#pragma once

#include <SDL3/SDL.h>
#include <string>
#include <vector>

#include "frontend_tabs.h"
#include "settings.h"
#include "src/library/game_library.h"
#include "src/database/game_database.h"
#include "src/collection/collection_manager.h"
#include "src/media/asset_manager.h"
#include "src/import/import_manager.h"
#include "src/metadata/metadata_engine.h"

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
    static constexpr int MetadataFieldCount = 14;

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
    MetadataEngine metadataEngine_;

    int settingsSelected_ = 0;
    FrontendTab activeTab_ = FrontendTab::Library;
    bool running_ = true;
    bool redraw_ = true;
    bool stickUpHeld_ = false;
    bool stickDownHeld_ = false;
    bool stickLeftHeld_ = false;
    bool stickRightHeld_ = false;

    bool metadataEditMode_ = false;
    bool metadataTextInput_ = false;
    int metadataSelected_ = 0;
    std::size_t metadataCaret_ = 0;
    GameInfo metadataWorkingCopy_;
    std::string metadataManualPath_;
    int libraryDescriptionScroll_ = 0;
    std::vector<ProjectPage> projectPages_;
    int projectPageIndex_ = 0;
    int projectPageScroll_ = 0;
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
    void ToggleMetadataTextInput();
    void EditCurrentMetadataField();
    bool IsLongMetadataField() const noexcept;
    std::string* CurrentMetadataField();
    const char* CurrentMetadataLabel() const;
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
    bool TryLibraryFavoriteAt(float x, float y, bool activate);
    bool TryProjectControlAt(float x, float y);
    bool TryExitButtonAt(float x, float y);
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
};
