#include "src/frontend/frontend_app.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "frontend_boxart.h"
#include "src/frontend/frontend_screenshot.h"
#include "frontend_layout.h"
#include "frontend_panels.h"
#include "frontend_statusbar.h"
#include "input_manager.h"
#include "metadata_text_editor.h"
#include "launcher.h"
#include "src/database/library_view.h"
#include "src/media/manual_preview.h"
#include "theme_win95.h"
#include "ui_font.h"
#include "videopac_font.h"
#include "vdc_stub.h"

namespace
{
    void DrawText(
        SDL_Renderer* renderer,
        float x,
        float y,
        float scale,
        const std::string& text)
    {
        UiFont_DrawText(renderer, x, y, 16.5f * scale, text);
    }

    std::vector<std::string> WrapProjectText(const std::string& text, int maxCharacters)
    {
        std::vector<std::string> lines;
        std::string paragraph;
        std::istringstream input(text);
        while (std::getline(input, paragraph))
        {
            if (!paragraph.empty() && paragraph.back() == '\r') paragraph.pop_back();
            if (paragraph.empty()) { lines.emplace_back(); continue; }
            std::istringstream words(paragraph);
            std::string word, line;
            while (words >> word)
            {
                if (line.empty()) line = word;
                else if (static_cast<int>(line.size() + 1 + word.size()) <= maxCharacters) line += " " + word;
                else { lines.push_back(line); line = word; }
            }
            if (!line.empty()) lines.push_back(line);
        }
        return lines;
    }

    void DrawSunkenFrame(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(
            renderer,
            rect.x,
            rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f,
            rect.y + rect.h - 1.0f);
        SDL_RenderLine(
            renderer,
            rect.x + rect.w - 1.0f,
            rect.y,
            rect.x + rect.w - 1.0f,
            rect.y + rect.h - 1.0f);
    }
}

FrontendApp::FrontendApp(SDL_Window* window)
    : window_(window)
{
}

FrontendApp::~FrontendApp()
{
    FrontendBoxArt_Shutdown();
    FrontendScreenshot_Shutdown();
    ManualPreview_Shutdown();
    FrontendLayout_Shutdown();
    UiFont_Shutdown();
}

bool FrontendApp::Initialize()
{
    if (!window_)
        return false;

    renderer_ = RefreshRenderer();

    if (!renderer_)
        return false;

    const char* basePath = SDL_GetBasePath();
    const std::string baseFolder = basePath ? basePath : "";

    settingsPath_ = baseFolder + "o2em-ng.cfg";
    settings_ = LoadSettings(settingsPath_);

    SDL_GetWindowPosition(window_, &windowedX_, &windowedY_);
    SDL_GetWindowSize(window_, &windowedWidth_, &windowedHeight_);
    haveWindowedBounds_ = true;

    if (settings_.start_fullscreen)
        ApplyFullscreenMode(true);
    else
        std::printf("O2EM-NG: windowed startup enabled from settings.\n");

    const std::vector<RomEntry> allRoms = LoadRoms(baseFolder + "ROMS");
    const std::vector<RomEntry> officialRoms =
        BuildLibraryView(allRoms, LibraryView::Official);
    library_.SetGames(officialRoms);

    // Resolve local assets and optional per-game metadata first. The database
    // is loaded last so user-owned edits always take precedence.
    assetManager_.SetBasePath(baseFolder);
    importManager_.SetBasePath(baseFolder);
    RefreshInstalledBiosFiles();
    assetManager_.Populate(library_);

    std::size_t manualCount = 0;
    for (const GameInfo& game : library_.Games())
    {
        if (!game.manual.empty())
            ++manualCount;

        std::printf(
            "O2EM-NG: media ID %02d | ROM: %s | Manual: %s\n",
            game.videopacNumber,
            game.filename.c_str(),
            game.manual.empty()
                ? "NOT FOUND"
                : game.manual.filename().string().c_str());
    }
    std::printf(
        "O2EM-NG: resolved manuals for %zu of %zu games from Manuals by ID.\n",
        manualCount,
        library_.Count());

    metadataEngine_.SetBasePath(baseFolder);
    const std::size_t metadataCount = metadataEngine_.Populate(library_);
    std::printf(
        "O2EM-NG: loaded metadata for %zu of %zu games.\n",
        metadataCount,
        library_.Count());

    gameDatabase_.SetBasePath(baseFolder);
    const GameDatabaseResult databaseResult =
        gameDatabase_.InitializeAndPopulate(library_);
    std::printf("O2EM-NG: %s\n", databaseResult.message.c_str());
    if (gameDatabase_.InitializeProjectPages())
        projectPages_ = gameDatabase_.LoadProjectPages();
    for (const std::string& filename : databaseResult.unmatchedRomFilenames)
        std::printf("O2EM-NG: unmatched ROM: %s\n", filename.c_str());

    // Patch 0011: Gamelist.txt contains titles/categories but not a complete
    // set of release facts. Fill only fields that are still empty after the
    // metadata engine and user database have both run.
    std::size_t fallbackCount = 0;
    for (GameInfo& game : library_.Games())
    {
        const std::string before = game.shortDescription + game.description +
            game.publisher + game.players + game.controls;
        ApplyCatalogFallbacks(game);
        const std::string after = game.shortDescription + game.description +
            game.publisher + game.players + game.controls;
        if (before != after)
            ++fallbackCount;

        std::printf(
            "O2EM-NG: metadata ID %02d | title=%s | year=%s | publisher=%s | source=%s\n",
            game.videopacNumber,
            game.title.c_str(),
            game.year.empty() ? "NOT AVAILABLE" : game.year.c_str(),
            game.publisher.empty() ? "NOT AVAILABLE" : game.publisher.c_str(),
            before != after ? "catalog fallback" : "metadata/database");
    }
    std::printf(
        "O2EM-NG: catalog fallbacks completed for %zu of %zu games.\n",
        fallbackCount, library_.Count());

    collections_.Attach(&library_);

    settingsSelected_ = 0;
    activeTab_ = FrontendTab::Library;
    running_ = true;
    redraw_ = true;

    return true;
}

bool FrontendApp::HandleEvent(const SDL_Event& event)
{
    InputManager_HandleEvent(event);

    switch (event.type)
    {
    case SDL_EVENT_QUIT:
        running_ = false;
        break;

    case SDL_EVENT_WINDOW_RESIZED:
        redraw_ = true;
        break;

    case SDL_EVENT_KEY_DOWN:
        HandleKeyDown(event.key);
        break;

    case SDL_EVENT_TEXT_INPUT:
        HandleTextInput(event.text);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
        HandleGamepadButtonDown(event.gbutton);
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        HandleGamepadAxisMotion(event.gaxis);
        break;

    case SDL_EVENT_MOUSE_BUTTON_DOWN:
        HandleMouseButtonDown(event.button);
        break;

    case SDL_EVENT_MOUSE_WHEEL:
        HandleMouseWheel(event.wheel);
        break;

    default:
        break;
    }

    return running_;
}

void FrontendApp::Draw()
{
    if (!redraw_ || !renderer_)
        return;

    DrawFrontend();
    redraw_ = false;
}

bool FrontendApp::IsRunning() const noexcept
{
    return running_;
}

void FrontendApp::RequestRedraw() noexcept
{
    redraw_ = true;
}

SDL_Renderer* FrontendApp::RefreshRenderer()
{
    if (!window_)
        return nullptr;

    VDCStub_SetWindow(window_);
    return SDL_GetRenderer(window_);
}

const GameInfo* FrontendApp::GetSelectedGame() const noexcept
{
    return collections_.Current();
}

GameInfo* FrontendApp::GetSelectedGame() noexcept
{
    return collections_.Current();
}

void FrontendApp::CycleCollectionView(int direction)
{
    if (activeTab_ != FrontendTab::Library)
        return;
    collections_.CycleView(direction);
    std::printf("O2EM-NG: collection view changed to %s (%zu games).\n",
        collections_.ViewName(), collections_.Count());
    redraw_ = true;
}

void FrontendApp::ToggleFavorite()
{
    if (activeTab_ != FrontendTab::Library && activeTab_ != FrontendTab::Cartridge)
        return;

    GameInfo* game = GetSelectedGame();
    if (!game)
        return;

    const bool newValue = !game->favorite;
    if (!gameDatabase_.SetFavorite(game->filename, newValue))
    {
        std::printf("O2EM-NG: could not update favorite for %s.\n", game->filename.c_str());
        return;
    }

    game->favorite = newValue;
    std::printf("O2EM-NG: %s %s.\n", newValue ? "favorited" : "unfavorited", game->title.c_str());
    collections_.Rebuild();
    redraw_ = true;
}

void FrontendApp::MoveSelection(int direction)
{
    if (collections_.Count() == 0)
        return;

    collections_.Move(direction);
    libraryDescriptionScroll_ = 0;
    redraw_ = true;
}

void FrontendApp::MoveSettingsSelection(int direction)
{
    settingsSelected_ += direction;

    if (settingsSelected_ < 0)
        settingsSelected_ = SettingsItemCount - 1;
    else if (settingsSelected_ >= SettingsItemCount)
        settingsSelected_ = 0;

    redraw_ = true;
}

void FrontendApp::MoveTab(int direction)
{
    const int tabCount = FrontendTabs_GetCount();
    int tabIndex = static_cast<int>(activeTab_) + direction;

    if (tabIndex < 0)
        tabIndex = tabCount - 1;
    else if (tabIndex >= tabCount)
        tabIndex = 0;

    SetActiveTab(static_cast<FrontendTab>(tabIndex));
}

void FrontendApp::SetActiveTab(FrontendTab tab)
{
    activeTab_ = tab;
    // Patch 0022a: only Import Center uses the full catalogue. Every other
    // tab keeps the Game Library limited to ROMs that are actually installed.
    collections_.SetShowUninstalled(tab == FrontendTab::Extras);
    if (tab == FrontendTab::About) SelectProjectPage(0);
    else if (tab == FrontendTab::Credits) SelectProjectPage(1);
    redraw_ = true;

    std::printf(
        "O2EM-NG: active tab changed to %s.\n",
        FrontendTabs_GetName(activeTab_));
}

void FrontendApp::ActivateSelection()
{
    if (activeTab_ == FrontendTab::Settings)
    {
        ActivateSettingsSelection();
        return;
    }

    if (activeTab_ == FrontendTab::Manual)
    {
        OpenSelectedManual();
        return;
    }

    if (activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits)
    {
        EditCurrentProjectPage();
        return;
    }

    if (activeTab_ == FrontendTab::Cartridge)
    {
        if (!metadataEditMode_)
            BeginMetadataEdit();
        else
            ToggleMetadataTextInput();
        return;
    }

    if (activeTab_ != FrontendTab::Library)
        return;

    GameInfo* game = GetSelectedGame();
    if (!game)
        return;

    if (!SelectedBiosExists())
    {
        const char* message = installedBiosFiles_.empty()
            ? "No BIOS is installed. Add a BIOS in Import Center before starting a game."
            : "The selected BIOS file is not installed. Select an installed BIOS in Quick Settings.";
        HWND owner = nullptr;
        if (window_)
        {
            owner = static_cast<HWND>(SDL_GetPointerProperty(
                SDL_GetWindowProperties(window_),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        }
        MessageBoxA(owner, message, "O2EM-NG - BIOS required",
            MB_OK | MB_ICONWARNING);
        importStatus_ = message;
        redraw_ = true;
        return;
    }

    const std::string launchedFilename = game->filename;

    // The emulator destroys the SDL renderer when a game closes. Release the
    // cached header texture while the frontend renderer is still valid, so the
    // texture can never survive as a stale pointer into the destroyed renderer.
    // FrontendLayout_DrawHeader() will reload it on the first frontend redraw.
    FrontendLayout_Shutdown();

    LaunchRom(window_, *game, settings_.region_mode, settings_.bios_file, settings_.scanlines);

    const std::time_t launchedAt = std::time(nullptr);
    if (gameDatabase_.RecordLaunch(launchedFilename, launchedAt))
    {
        ++game->playCount;
        game->lastPlayed = launchedAt;
        collections_.Rebuild();
    }

    renderer_ = RefreshRenderer();
    redraw_ = true;
}

void FrontendApp::ApplyFullscreenMode(bool enabled)
{
    if (!window_)
        return;

    const bool currentlyFullscreen =
        (SDL_GetWindowFlags(window_) & SDL_WINDOW_FULLSCREEN) != 0;

    if (enabled == currentlyFullscreen)
    {
        redraw_ = true;
        return;
    }

    if (enabled)
    {
        SDL_GetWindowPosition(window_, &windowedX_, &windowedY_);
        SDL_GetWindowSize(window_, &windowedWidth_, &windowedHeight_);
        haveWindowedBounds_ = true;
    }

    if (!SDL_SetWindowFullscreen(window_, enabled))
    {
        std::printf(
            "O2EM-NG: SDL_SetWindowFullscreen(%s) failed: %s\n",
            enabled ? "true" : "false",
            SDL_GetError());
        settings_.start_fullscreen = currentlyFullscreen;
        return;
    }

    if (!enabled && haveWindowedBounds_)
    {
        SDL_SetWindowPosition(window_, windowedX_, windowedY_);
        SDL_SetWindowSize(window_, windowedWidth_, windowedHeight_);
    }

    renderer_ = RefreshRenderer();
    libraryDescriptionScroll_ = 0;
    redraw_ = true;
    std::printf("O2EM-NG: fullscreen changed live: %s.\n",
        enabled ? "ON" : "OFF");
}

void FrontendApp::ActivateSettingsSelection()
{
    switch (settingsSelected_)
    {
    case 0:
        settings_.start_fullscreen = !settings_.start_fullscreen;
        ApplyFullscreenMode(settings_.start_fullscreen);
        break;

    case 1:
        if (settings_.region_mode == RegionMode::Auto)
            settings_.region_mode = RegionMode::PAL;
        else if (settings_.region_mode == RegionMode::PAL)
            settings_.region_mode = RegionMode::NTSC;
        else
            settings_.region_mode = RegionMode::Auto;
        break;

    case 2:
        settings_.scanlines = !settings_.scanlines;
        break;

    case 3:
        RefreshInstalledBiosFiles();
        if (installedBiosFiles_.empty())
        {
            settings_.bios_file.clear();
            importStatus_ = "No BIOS installed. Add one in Import Center.";
            break;
        }
        else
        {
            auto current = std::find(installedBiosFiles_.begin(),
                installedBiosFiles_.end(), settings_.bios_file);
            if (current == installedBiosFiles_.end() || ++current == installedBiosFiles_.end())
                settings_.bios_file = installedBiosFiles_.front();
            else
                settings_.bios_file = *current;
        }
        break;

    default:
        return;
    }

    SaveSettings(settingsPath_, settings_);
    std::printf("O2EM-NG: Settings saved.\n");
    redraw_ = true;
}

void FrontendApp::GoBack()
{
    if (metadataEditMode_)
    {
        CancelMetadataEdit();
        return;
    }

    if (activeTab_ != FrontendTab::Library)
    {
        SetActiveTab(FrontendTab::Library);
        return;
    }

    running_ = false;
}

void FrontendApp::HandleKeyDown(const SDL_KeyboardEvent& event)
{
    if (event.repeat)
        return;

    if (metadataEditMode_ && metadataTextInput_)
    {
        const bool controlDown = (event.mod & SDL_KMOD_CTRL) != 0;
        const bool shiftDown = (event.mod & SDL_KMOD_SHIFT) != 0;

        // SDL text input deliberately does not deliver clipboard shortcuts.
        // Handle Ctrl+V and the traditional Shift+Insert shortcut here.
        if ((controlDown && event.key == SDLK_V) ||
            (shiftDown && event.key == SDLK_INSERT))
        {
            std::string* field = CurrentMetadataField();
            if (field && SDL_HasClipboardText())
            {
                char* clipboardText = SDL_GetClipboardText();
                if (clipboardText)
                {
                    field->append(clipboardText);
                    SDL_free(clipboardText);
                    redraw_ = true;
                }
            }
            return;
        }

        std::string* field = CurrentMetadataField();
        if (field) metadataCaret_ = (std::min)(metadataCaret_, field->size());

        if (event.key == SDLK_BACKSPACE)
        {
            if (field && metadataCaret_ > 0)
            {
                field->erase(metadataCaret_ - 1, 1);
                --metadataCaret_;
            }
            redraw_ = true;
        }
        else if (event.key == SDLK_DELETE)
        {
            if (field && metadataCaret_ < field->size())
                field->erase(metadataCaret_, 1);
            redraw_ = true;
        }
        else if (event.key == SDLK_LEFT)
        {
            if (metadataCaret_ > 0) --metadataCaret_;
            redraw_ = true;
        }
        else if (event.key == SDLK_RIGHT)
        {
            if (field && metadataCaret_ < field->size()) ++metadataCaret_;
            redraw_ = true;
        }
        else if (event.key == SDLK_HOME)
        {
            metadataCaret_ = 0;
            redraw_ = true;
        }
        else if (event.key == SDLK_END)
        {
            if (field) metadataCaret_ = field->size();
            redraw_ = true;
        }
        else if (event.key == SDLK_RETURN || event.key == SDLK_ESCAPE)
            ToggleMetadataTextInput();
        return;
    }

    if (metadataEditMode_)
    {
        if ((event.mod & SDL_KMOD_CTRL) && event.key == SDLK_S) { SaveMetadataEdit(); return; }
        if (event.key == SDLK_ESCAPE) { CancelMetadataEdit(); return; }
        if (event.key == SDLK_UP) { MoveMetadataSelection(-1); return; }
        if (event.key == SDLK_DOWN) { MoveMetadataSelection(1); return; }
        if (event.key == SDLK_RETURN) { EditCurrentMetadataField(); return; }
    }

    switch (event.key)
    {
    case SDLK_ESCAPE: GoBack(); break;
    case SDLK_LEFT: MoveTab(-1); break;
    case SDLK_RIGHT: MoveTab(1); break;
    case SDLK_TAB: MoveTab(1); break;
    case SDLK_UP:
        if (activeTab_ == FrontendTab::Settings) MoveSettingsSelection(-1);
        else MoveSelection(-1);
        break;
    case SDLK_DOWN:
        if (activeTab_ == FrontendTab::Settings) MoveSettingsSelection(1);
        else MoveSelection(1);
        break;
    case SDLK_RETURN: ActivateSelection(); break;
    case SDLK_E:
        if (activeTab_ == FrontendTab::Cartridge) BeginMetadataEdit();
        else if (activeTab_ == FrontendTab::Library) EditLibraryDescription();
        else if (activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits) EditCurrentProjectPage();
        break;
    case SDLK_F: ToggleFavorite(); break;
    case SDLK_1: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Rom); break;
    case SDLK_2: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Bios); break;
    case SDLK_3: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Manual); break;
    case SDLK_4: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Cover); break;
    case SDLK_5: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Screenshot); break;
    case SDLK_PAGEUP:
        if (activeTab_ == FrontendTab::Screenshot)
        {
            FrontendScreenshot_Move(GetSelectedGame(), -1);
            redraw_ = true;
        }
        else if (activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits)
            SelectProjectPage(projectPageIndex_ - 1);
        else
            CycleCollectionView(-1);
        break;
    case SDLK_PAGEDOWN:
        if (activeTab_ == FrontendTab::Screenshot)
        {
            FrontendScreenshot_Move(GetSelectedGame(), 1);
            redraw_ = true;
        }
        else if (activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits)
            SelectProjectPage(projectPageIndex_ + 1);
        else
            CycleCollectionView(1);
        break;
    default: break;
    }
}

void FrontendApp::HandleTextInput(const SDL_TextInputEvent& event)
{
    if (!metadataEditMode_ || !metadataTextInput_)
        return;
    std::string* field = CurrentMetadataField();
    if (field && event.text)
    {
        metadataCaret_ = (std::min)(metadataCaret_, field->size());
        field->insert(metadataCaret_, event.text);
        metadataCaret_ += std::char_traits<char>::length(event.text);
        redraw_ = true;
    }
}

void FrontendApp::HandleGamepadButtonDown(
    const SDL_GamepadButtonEvent& event)
{
    switch (event.button)
    {
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT:
        MoveTab(-1);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT:
        MoveTab(1);
        break;

    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        CycleCollectionView(-1);
        break;

    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER:
        CycleCollectionView(1);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_UP:
        if (activeTab_ == FrontendTab::Settings)
            MoveSettingsSelection(-1);
        else
            MoveSelection(-1);
        break;

    case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
        if (activeTab_ == FrontendTab::Settings)
            MoveSettingsSelection(1);
        else
            MoveSelection(1);
        break;

    case SDL_GAMEPAD_BUTTON_SOUTH:
        ActivateSelection();
        break;

    case SDL_GAMEPAD_BUTTON_EAST:
        GoBack();
        break;

    // Xbox Y / north is intentionally not handled in the frontend.
    // During emulation it remains reserved for controller-port switching.
    case SDL_GAMEPAD_BUTTON_WEST:
        ToggleFavorite();
        break;

    default:
        break;
    }
}

void FrontendApp::HandleGamepadAxisMotion(
    const SDL_GamepadAxisEvent& event)
{
    if (event.axis == SDL_GAMEPAD_AXIS_LEFTY)
    {
        if (event.value < -StickDeadzone)
        {
            if (!stickUpHeld_)
            {
                if (activeTab_ == FrontendTab::Settings)
                    MoveSettingsSelection(-1);
                else
                    MoveSelection(-1);
            }

            stickUpHeld_ = true;
            stickDownHeld_ = false;
        }
        else if (event.value > StickDeadzone)
        {
            if (!stickDownHeld_)
            {
                if (activeTab_ == FrontendTab::Settings)
                    MoveSettingsSelection(1);
                else
                    MoveSelection(1);
            }

            stickDownHeld_ = true;
            stickUpHeld_ = false;
        }
        else
        {
            stickUpHeld_ = false;
            stickDownHeld_ = false;
        }
    }
    else if (event.axis == SDL_GAMEPAD_AXIS_LEFTX)
    {
        if (event.value < -StickDeadzone)
        {
            if (!stickLeftHeld_)
                MoveTab(-1);

            stickLeftHeld_ = true;
            stickRightHeld_ = false;
        }
        else if (event.value > StickDeadzone)
        {
            if (!stickRightHeld_)
                MoveTab(1);

            stickRightHeld_ = true;
            stickLeftHeld_ = false;
        }
        else
        {
            stickLeftHeld_ = false;
            stickRightHeld_ = false;
        }
    }
}

void FrontendApp::HandleMouseButtonDown(const SDL_MouseButtonEvent& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;
    if (TryExitButtonAt(event.x, event.y)) return;
    if (TrySelectTabAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Manual && TryActivateManualAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Screenshot)
    {
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
        if (FrontendScreenshot_DeleteHitTest(panels.rightContent, GetSelectedGame(),
                event.x, event.y))
        {
            RunDelete(ImportAssetType::Screenshot);
            return;
        }
        if (FrontendScreenshot_HitTest(panels.rightContent, GetSelectedGame(),
                event.x, event.y))
        {
            redraw_ = true;
            return;
        }
    }
    if ((activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits) && TryProjectControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Extras && TryImportControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Cartridge && TryMetadataControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Library && TryLibraryFavoriteAt(event.x, event.y, event.clicks >= 2)) return;
    if (activeTab_ == FrontendTab::Library && TryLibraryQuickControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Library && event.clicks >= 2 &&
        TryEditLibraryDescriptionAt(event.x, event.y)) return;
    const bool activate = event.clicks >= 2;
    if (TrySelectLibraryRowAt(event.x, event.y, activate)) return;
    TrySelectSettingsRowAt(event.x, event.y, true);
}

void FrontendApp::HandleMouseWheel(const SDL_MouseWheelEvent& event)
{
    if (event.y == 0.0f)
        return;

    if (activeTab_ == FrontendTab::Settings)
    {
        MoveSettingsSelection(event.y > 0.0f ? -1 : 1);
        return;
    }

    if (activeTab_ == FrontendTab::About || activeTab_ == FrontendTab::Credits)
    {
        projectPageScroll_ += event.y > 0.0f ? -3 : 3;
        projectPageScroll_ = (std::max)(0, projectPageScroll_);
        redraw_ = true;
        return;
    }

    if (activeTab_ == FrontendTab::Library)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_GetMouseState(&mouseX, &mouseY);

        int windowWidth = 0;
        int windowHeight = 0;
        SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
        const FrontendPanelLayout panels =
            FrontendPanels_Calculate(windowWidth, windowHeight);

        const float descriptionTop = panels.rightContent.y +
            panels.rightContent.h * 0.64f;
        if (mouseX >= panels.rightContent.x &&
            mouseX < panels.rightContent.x + panels.rightContent.w &&
            mouseY >= descriptionTop &&
            mouseY < panels.rightContent.y + panels.rightContent.h)
        {
            libraryDescriptionScroll_ += event.y > 0.0f ? -3 : 3;
            libraryDescriptionScroll_ = (std::max)(0, libraryDescriptionScroll_);
            redraw_ = true;
            return;
        }
    }

    MoveSelection(event.y > 0.0f ? -1 : 1);
}

bool FrontendApp::TryExitButtonAt(float x, float y)
{
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);

    if (!FrontendStatusBar_HitTestExit(
            windowWidth, windowHeight, x, y))
        return false;

    running_ = false;
    return true;
}

bool FrontendApp::TrySelectTabAt(float x, float y)
{
    constexpr float barY = 142.0f;
    constexpr float barH = 42.0f;
    constexpr float marginX = 22.0f;
    constexpr float gap = 6.0f;

    if (y < barY || y >= barY + barH)
        return false;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    (void)windowHeight;

    const int tabCount = FrontendTabs_GetCount();
    const float availableWidth = static_cast<float>(windowWidth) -
        (marginX * 2.0f) - (gap * static_cast<float>(tabCount - 1));
    const float tabWidth = availableWidth / static_cast<float>(tabCount);

    for (int index = 0; index < tabCount; ++index)
    {
        const float tabX = marginX + static_cast<float>(index) * (tabWidth + gap);
        if (x >= tabX && x < tabX + tabWidth)
        {
            SetActiveTab(static_cast<FrontendTab>(index));
            return true;
        }
    }

    return false;
}

bool FrontendApp::TrySelectLibraryRowAt(float x, float y, bool activate)
{
    if (activeTab_ == FrontendTab::Settings || collections_.Count() == 0)
        return false;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(windowWidth, windowHeight);
    const SDL_FRect& content = panels.leftContent;

    if (x < content.x || x >= content.x + content.w ||
        y < content.y + 41.0f || y >= content.y + content.h)
    {
        return false;
    }

    const int selected = static_cast<int>(collections_.CurrentPosition());
    int firstVisible = (std::max)(0, selected - (VisibleRows / 2));
    const int itemCount = static_cast<int>(collections_.Count());
    firstVisible = (std::min)(firstVisible, (std::max)(0, itemCount - VisibleRows));

    const float firstRowY = content.y + 48.0f;
    const int row = static_cast<int>((y - (firstRowY - 7.0f)) / 32.0f);
    if (row < 0 || row >= VisibleRows)
        return false;

    const int position = firstVisible + row;
    if (position < 0 || position >= itemCount)
        return false;

    collections_.SelectPosition(static_cast<std::size_t>(position));
    redraw_ = true;

    if (activate)
        ActivateSelection();

    return true;
}

bool FrontendApp::TrySelectSettingsRowAt(float x, float y, bool activate)
{
    if (activeTab_ != FrontendTab::Settings)
        return false;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(windowWidth, windowHeight);
    const SDL_FRect& content = panels.rightContent;

    const float innerX = content.x + 18.0f;
    const float innerY = content.y + 18.0f;
    const float innerW = content.w - 36.0f;
    const float rowY[SettingsItemCount] = {
        innerY + 75.0f,
        innerY + 135.0f,
        innerY + 195.0f,
        innerY + 255.0f
    };

    if (x < innerX || x >= innerX + innerW)
        return false;

    for (int index = 0; index < SettingsItemCount; ++index)
    {
        // Match the complete highlighted row, not only the rendered text.
        if (y >= rowY[index] - 12.0f && y < rowY[index] + 30.0f)
        {
            settingsSelected_ = index;
            redraw_ = true;
            if (activate)
                ActivateSettingsSelection();
            return true;
        }
    }

    return false;
}

void FrontendApp::DrawFrontend()
{
    if (!window_ || !renderer_)
        return;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);

    SDL_SetRenderDrawColor(renderer_, 10, 10, 14, 255);
    SDL_RenderClear(renderer_);

    FrontendLayout_DrawHeader(window_, renderer_);
    FrontendTabs_Draw(renderer_, windowWidth, activeTab_);

    const FrontendPanelLayout panels =
        FrontendPanels_Calculate(windowWidth, windowHeight);

    FrontendPanels_Draw(renderer_, panels);
    DrawLibraryList(panels.leftContent);
    DrawActiveTab(panels.rightContent);

    std::string status = "Tab: ";
    status += FrontendTabs_GetName(activeTab_);
    if (activeTab_ == FrontendTab::Extras)
    {
        status += "  |  Catalog: ALL TITLES  |  Select a title and use ADD to install media";
    }
    else
    {
        status += "  |  View: ";
        status += collections_.ViewName();
        status += "  |  PgUp/PgDn or LB/RB: view  |  F/X: favorite  |  Enter/A: play  |  B: back/exit";
    }

    int installedGameCount = 0;
    for (const GameInfo& game : library_.Games())
    {
        if (!game.romPath.empty() || !game.rom.path.empty())
            ++installedGameCount;
    }

    FrontendStatusBar_Draw(
        renderer_,
        windowWidth,
        windowHeight,
        installedGameCount,
        status.c_str());

    SDL_RenderPresent(renderer_);
}

void FrontendApp::DrawLibraryList(const SDL_FRect& content)
{
    const float panelX = content.x;
    const float panelY = content.y;
    const float panelWidth = content.w;

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    const std::string heading = activeTab_ == FrontendTab::Extras
        ? "GAME CATALOG - SELECT GAME TO IMPORT"
        : std::string("GAME LIBRARY - ") + collections_.ViewName();
    DrawText(renderer_, panelX + 18.0f, panelY + 12.0f, 1.15f, heading);

    if (collections_.Count() == 0)
    {
        DrawText(renderer_, panelX + 30.0f, panelY + 55.0f, 1.25f, "NO GAMES IN THIS VIEW");
        return;
    }

    const int selected = static_cast<int>(collections_.CurrentPosition());
    int firstVisible = (std::max)(0, selected - (VisibleRows / 2));
    const int itemCount = static_cast<int>(collections_.Count());
    firstVisible = (std::min)(firstVisible, (std::max)(0, itemCount - VisibleRows));
    const int lastVisible = (std::min)(firstVisible + VisibleRows, itemCount);

    float y = panelY + 48.0f;
    constexpr float listScale = 1.18f;
    for (int index = firstVisible; index < lastVisible; ++index)
    {
        if (index == selected)
        {
            SDL_SetRenderDrawColor(renderer_, 185, 35, 35, 255);
            const SDL_FRect highlight{ panelX + 15.0f, y - 6.0f, panelWidth - 30.0f, 27.0f };
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        }
        else Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

        const GameInfo* game = collections_.Get(static_cast<std::size_t>(index));
        const int number = game && game->videopacNumber > 0 ? game->videopacNumber : index + 1;
        char prefix[16]{};
        std::snprintf(prefix, sizeof(prefix), "%02d  ", number);
        std::string line;
        if (activeTab_ == FrontendTab::Extras)
        {
            const bool installed = game &&
                (!game->romPath.empty() || !game->rom.path.empty());
            line += installed ? "[x] " : "[ ] ";
        }
        line += prefix;
        if (game && game->favorite) line += "* ";
        line += game ? game->title : std::string();
        if (game && collections_.View() == CollectionView::MostPlayed)
            line += "  [" + std::to_string(game->playCount) + "]";

        const float availableTextWidth = (std::max)(0.0f, panelWidth - 62.0f);
        const std::size_t maximumCharacters = static_cast<std::size_t>(availableTextWidth / (8.0f * listScale));
        if (maximumCharacters >= 4 && line.size() > maximumCharacters)
            line = line.substr(0, maximumCharacters - 3) + "...";
        DrawText(renderer_, panelX + 30.0f, y, listScale, line);
        y += 30.0f;
    }
}

void FrontendApp::DrawLibraryDashboard(const SDL_FRect& content)
{
    const GameInfo* game = GetSelectedGame();
    const float margin = 12.0f;
    const float gap = 10.0f;
    const float topHeight = (std::max)(250.0f, content.h * 0.61f);
    const float sideWidth = (std::clamp)(content.w * 0.22f, 210.0f, 285.0f);
    const float infoWidth = (std::clamp)(content.w * 0.30f, 270.0f, 370.0f);

    const SDL_FRect coverFrame{ content.x + margin, content.y + margin,
        content.w - infoWidth - sideWidth - margin * 2.0f - gap * 2.0f, topHeight - margin };
    const SDL_FRect infoFrame{ coverFrame.x + coverFrame.w + gap, coverFrame.y, infoWidth, coverFrame.h };
    const SDL_FRect sideFrame{ infoFrame.x + infoFrame.w + gap, coverFrame.y, sideWidth, coverFrame.h };
    const float quickHeight = sideFrame.h * 0.52f;
    const SDL_FRect quickFrame{ sideFrame.x, sideFrame.y, sideFrame.w, quickHeight };
    const SDL_FRect favoritesFrame{ sideFrame.x, sideFrame.y + quickHeight + gap, sideFrame.w, sideFrame.h - quickHeight - gap };
    const SDL_FRect descriptionFrame{ content.x + margin, content.y + topHeight + gap,
        content.w - margin * 2.0f, content.h - topHeight - gap - margin };

    DrawSunkenFrame(renderer_, coverFrame);
    DrawSunkenFrame(renderer_, infoFrame);
    DrawSunkenFrame(renderer_, quickFrame);
    DrawSunkenFrame(renderer_, favoritesFrame);
    DrawSunkenFrame(renderer_, descriptionFrame);

    const SDL_FRect coverInner{
        coverFrame.x + 5.0f,
        coverFrame.y + 28.0f,
        coverFrame.w - 10.0f,
        coverFrame.h - 33.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &coverInner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, coverFrame.x + 10.0f, coverFrame.y + 7.0f, 1.05f, "COVER / MEDIA");
    FrontendBoxArt_DrawImage(renderer_, coverInner, game);

    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    const SDL_FRect infoInner{
        infoFrame.x + 5.0f,
        infoFrame.y + 28.0f,
        infoFrame.w - 10.0f,
        infoFrame.h - 33.0f
    };
    SDL_RenderFillRect(renderer_, &infoInner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, infoFrame.x + 10.0f, infoFrame.y + 7.0f, 1.05f, "GAME INFORMATION");

    if (!game)
    {
        DrawText(renderer_, infoInner.x + 16.0f, infoInner.y + 20.0f, 1.15f, "SELECT A GAME");
        return;
    }

    const auto valueOrDash = [](const std::string& value) -> std::string
    {
        return value.empty() ? "-" : value;
    };
    const auto fitText = [](const std::string& value, float width, float scale)
    {
        const std::size_t maximumCharacters = static_cast<std::size_t>(
            (std::max)(1.0f, width) / (8.0f * scale));
        if (value.size() <= maximumCharacters)
            return value;
        if (maximumCharacters <= 3)
            return value.substr(0, maximumCharacters);
        return value.substr(0, maximumCharacters - 3) + "...";
    };

    float y = infoInner.y + 16.0f;
    DrawText(renderer_, infoInner.x + 15.0f, y, 1.30f,
        fitText(game->title, infoInner.w - 30.0f, 1.30f));
    y += 36.0f;

    const std::string number = game->videopacNumber > 0
        ? "Videopac No.: " + std::to_string(game->videopacNumber)
        : "Videopac No.: -";
    const std::string rows[] = {
        number,
        "Publisher: " + valueOrDash(game->publisher),
        "Developer: " + valueOrDash(game->developer),
        "Year: " + valueOrDash(game->year),
        "Genre: " + valueOrDash(game->genre),
        "Players: " + valueOrDash(game->players),
        "Controls: " + valueOrDash(game->controls),
        "Voice Module: " + valueOrDash(game->voiceModule),
        "Videopac+: " + valueOrDash(game->videopacPlus),
        "Rating: " + valueOrDash(game->rating),
        std::string("Manual: ") + (game->manual.empty() ? "No" : "Available"),
        "Screenshots: " + std::to_string(game->screenshots.size()),
        std::string("Favorite: ") + (game->favorite ? "Yes" : "No")
    };

    for (const std::string& row : rows)
    {
        if (y > infoInner.y + infoInner.h - 20.0f)
            break;
        DrawText(renderer_, infoInner.x + 15.0f, y, 0.96f,
            fitText(row, infoInner.w - 30.0f, 0.96f));
        y += 25.0f;
    }

    // Quick Settings uses familiar Win95-style controls instead of clickable text rows.
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, quickFrame.x + 10.0f, quickFrame.y + 7.0f, 1.05f, "QUICK SETTINGS");
    const SDL_FRect quickInner{quickFrame.x + 5.0f, quickFrame.y + 28.0f, quickFrame.w - 10.0f, quickFrame.h - 33.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &quickInner);

    const auto drawCombo = [&](float yPos, const char* label, const std::string& value)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, quickInner.x + 10.0f, yPos, 0.82f, label);
        const SDL_FRect box{quickInner.x + 10.0f, yPos + 17.0f, quickInner.w - 20.0f, 25.0f};
        DrawSunkenFrame(renderer_, box);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
        const SDL_FRect fill{box.x + 2.0f, box.y + 2.0f, box.w - 23.0f, box.h - 4.0f};
        SDL_RenderFillRect(renderer_, &fill);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, fill.x + 5.0f, fill.y + 2.0f, 0.82f, value);
        const SDL_FRect arrowButton{box.x + box.w - 21.0f, box.y + 2.0f, 19.0f, box.h - 4.0f};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &arrowButton);
        DrawText(renderer_, arrowButton.x + 5.0f, arrowButton.y + 1.0f, 0.80f, "v");
    };

    drawCombo(quickInner.y + 9.0f, "BIOS",
        settings_.bios_file.empty() ? "No BIOS installed" : settings_.bios_file);
    std::string regionText = RegionModeToString(settings_.region_mode);
    std::transform(regionText.begin(), regionText.end(), regionText.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    drawCombo(quickInner.y + 62.0f, "REGION", regionText);

    const float checkY = quickInner.y + 122.0f;
    const SDL_FRect checkBox{quickInner.x + 11.0f, checkY, 17.0f, 17.0f};
    DrawSunkenFrame(renderer_, checkBox);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    const SDL_FRect checkFill{checkBox.x + 2.0f, checkBox.y + 2.0f, checkBox.w - 4.0f, checkBox.h - 4.0f};
    SDL_RenderFillRect(renderer_, &checkFill);
    if (settings_.scanlines)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, checkBox.x + 2.0f, checkBox.y - 2.0f, 0.82f, "x");
    }
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, checkBox.x + 25.0f, checkY - 1.0f, 0.86f, "Scanlines");

    // Favorites are read from the live library state and sorted alphabetically.
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, favoritesFrame.x + 10.0f, favoritesFrame.y + 7.0f, 1.05f, "FAVORITES");
    const SDL_FRect favInner{favoritesFrame.x + 5.0f, favoritesFrame.y + 28.0f, favoritesFrame.w - 10.0f, favoritesFrame.h - 33.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &favInner);

    std::vector<const GameInfo*> favorites;
    for (const GameInfo& candidate : library_.Games())
        if (candidate.favorite) favorites.push_back(&candidate);
    std::stable_sort(favorites.begin(), favorites.end(), [](const GameInfo* a, const GameInfo* b)
    {
        return a->title < b->title;
    });

    const int maximumRows = (std::max)(1, static_cast<int>((favInner.h - 12.0f) / 24.0f));
    float favY = favInner.y + 13.0f;
    const int shown = (std::min)(maximumRows, static_cast<int>(favorites.size()));
    for (int i = 0; i < shown; ++i)
    {
        const GameInfo& favorite = *favorites[static_cast<std::size_t>(i)];
        if (game && favorite.filename == game->filename)
        {
            SDL_SetRenderDrawColor(renderer_, 185, 35, 35, 255);
            const SDL_FRect highlight{favInner.x + 5.0f, favY - 4.0f, favInner.w - 10.0f, 22.0f};
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        }
        std::string favLine = favorite.videopacNumber > 0
            ? (favorite.videopacNumber < 10 ? "0" : "") + std::to_string(favorite.videopacNumber)
            : "--";
        favLine += "  " + favorite.title;
        DrawText(renderer_, favInner.x + 12.0f, favY, 0.84f,
            fitText(favLine, favInner.w - 24.0f, 0.84f));
        favY += 24.0f;
    }
    if (favorites.empty())
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, favInner.x + 12.0f, favY, 0.84f, "No favorites added");
    }
    else if (static_cast<int>(favorites.size()) > shown)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, favInner.x + 12.0f, favInner.y + favInner.h - 20.0f,
            0.72f, "+ " + std::to_string(favorites.size() - static_cast<std::size_t>(shown)) + " more");
    }

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, descriptionFrame.x + 10.0f, descriptionFrame.y + 7.0f,
        1.05f, "GAME DESCRIPTION  (wheel: scroll | E/double-click: edit)");

    const SDL_FRect textArea{
        descriptionFrame.x + 8.0f,
        descriptionFrame.y + 30.0f,
        descriptionFrame.w - 24.0f,
        descriptionFrame.h - 38.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &textArea);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

    std::string description = game->description;
    if (description.empty())
        description = game->shortDescription;
    if (description.empty())
        description = "No game description available. Open Cartridge and choose Edit Info to add one.";

    constexpr float textScale = 1.0f;
    const std::size_t maxChars = static_cast<std::size_t>((std::max)(20.0f,
        textArea.w - 30.0f) / (8.0f * textScale));
    std::vector<std::string> lines;
    std::string paragraph;

    const auto flushParagraph = [&]()
    {
        if (paragraph.empty())
        {
            lines.emplace_back();
            return;
        }

        std::size_t start = 0;
        while (start < paragraph.size())
        {
            std::size_t count = (std::min)(maxChars, paragraph.size() - start);
            std::size_t end = start + count;
            if (end < paragraph.size())
            {
                const std::size_t space = paragraph.rfind(' ', end);
                if (space != std::string::npos && space > start)
                    end = space;
            }
            lines.push_back(paragraph.substr(start, end - start));
            start = end;
            while (start < paragraph.size() && paragraph[start] == ' ')
                ++start;
        }
    };

    for (char ch : description)
    {
        if (ch == '\r')
            continue;
        if (ch == '\n')
        {
            flushParagraph();
            paragraph.clear();
        }
        else
            paragraph.push_back(ch);
    }
    if (!paragraph.empty())
        flushParagraph();

    const int visibleLines = (std::max)(1,
        static_cast<int>((textArea.h - 12.0f) / 23.0f));
    const int maxScroll = (std::max)(0,
        static_cast<int>(lines.size()) - visibleLines);
    libraryDescriptionScroll_ = (std::clamp)(libraryDescriptionScroll_, 0, maxScroll);

    float textY = textArea.y + 8.0f;
    for (int index = libraryDescriptionScroll_;
         index < static_cast<int>(lines.size()) &&
         index < libraryDescriptionScroll_ + visibleLines;
         ++index)
    {
        DrawText(renderer_, textArea.x + 8.0f, textY, textScale, lines[index]);
        textY += 23.0f;
    }

    if (maxScroll > 0)
    {
        const SDL_FRect track{
            descriptionFrame.x + descriptionFrame.w - 13.0f,
            textArea.y,
            6.0f,
            textArea.h
        };
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &track);
        const float thumbHeight = (std::max)(18.0f,
            track.h * static_cast<float>(visibleLines) /
            static_cast<float>(lines.size()));
        const float thumbY = track.y +
            (track.h - thumbHeight) *
            static_cast<float>(libraryDescriptionScroll_) /
            static_cast<float>(maxScroll);
        const SDL_FRect thumb{ track.x, thumbY, track.w, thumbHeight };
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderFillRect(renderer_, &thumb);
    }
}

void FrontendApp::SelectProjectPage(int index)
{
    if (projectPages_.empty()) return;
    const int count = static_cast<int>(projectPages_.size());
    while (index < 0) index += count;
    projectPageIndex_ = index % count;
    projectPageScroll_ = 0;
    redraw_ = true;
}

void FrontendApp::EditCurrentProjectPage()
{
    if (projectPages_.empty()) return;
    ProjectPage& page = projectPages_[projectPageIndex_];
    std::string edited = page.content;
    if (!OpenMetadataTextEditor(page.title.c_str(), true, edited)) return;
    page.content = edited;
    if (!gameDatabase_.SaveProjectPage(page))
        std::printf("O2EM-NG: failed to save project page %s.\n", page.pageKey.c_str());
    projectPageScroll_ = 0;
    redraw_ = true;
}

bool FrontendApp::TryProjectControlAt(float x, float y)
{
    int windowWidth = 0, windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(windowWidth, windowHeight);
    const SDL_FRect content = panels.rightContent;
    const float margin = 14.0f;
    const SDL_FRect inner{content.x + margin + 4.0f, content.y + margin + 4.0f,
        content.w - margin * 2.0f - 8.0f, content.h - margin * 2.0f - 8.0f};
    const float gap = 4.0f;
    const float width = (inner.w - 24.0f - gap * 5.0f) / 6.0f;
    for (int i = 0; i < static_cast<int>(projectPages_.size()) && i < 6; ++i)
    {
        SDL_FRect button{inner.x + 12.0f + i * (width + gap), inner.y + 12.0f, width, 30.0f};
        if (x >= button.x && x < button.x + button.w && y >= button.y && y < button.y + button.h)
        { SelectProjectPage(i); return true; }
    }
    SDL_FRect edit{inner.x + inner.w - 118.0f, inner.y + 57.0f, 92.0f, 30.0f};
    if (x >= edit.x && x < edit.x + edit.w && y >= edit.y && y < edit.y + edit.h)
    { EditCurrentProjectPage(); return true; }
    return false;
}

void FrontendApp::DrawActiveTab(const SDL_FRect& content)
{
    switch (activeTab_)
    {
    case FrontendTab::Library:
        DrawLibraryDashboard(content);
        break;

    case FrontendTab::Cartridge:
        DrawGameInformationTab(content);
        break;

    case FrontendTab::Extras:
        DrawImportCenter(content);
        break;

    case FrontendTab::Screenshot:
        FrontendScreenshot_Draw(renderer_, content, GetSelectedGame());
        break;

    case FrontendTab::Manual:
        DrawManualTab(content);
        break;

    case FrontendTab::Settings:
        DrawSettingsTab(content);
        break;

    case FrontendTab::About:
        DrawAboutTab(content);
        break;

    case FrontendTab::Credits:
        DrawCreditsTab(content);
        break;

    default:
        break;
    }
}


void FrontendApp::EditLibraryDescription()
{
    GameInfo* game = GetSelectedGame();
    if (!game)
        return;

    std::string editedDescription = game->description;
    if (!OpenMetadataTextEditor("Game Description", true, editedDescription))
        return;

    game->description = editedDescription;
    if (!gameDatabase_.SaveUserMetadata(*game))
    {
        std::printf("O2EM-NG: failed to save Game Description for %s.\n",
            game->filename.c_str());
        return;
    }

    libraryDescriptionScroll_ = 0;
    collections_.Rebuild();
    std::printf("O2EM-NG: Game Description saved for %s.\n",
        game->filename.c_str());
    redraw_ = true;
}

bool FrontendApp::TryLibraryQuickControlAt(float x, float y)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
    const float margin = 12.0f;
    const float gap = 10.0f;
    const float topHeight = (std::max)(250.0f, panels.rightContent.h * 0.61f);
    const float sideWidth = (std::clamp)(panels.rightContent.w * 0.22f, 210.0f, 285.0f);
    const float infoWidth = (std::clamp)(panels.rightContent.w * 0.30f, 270.0f, 370.0f);
    const float coverWidth = panels.rightContent.w - infoWidth - sideWidth -
        margin * 2.0f - gap * 2.0f;
    const SDL_FRect quickFrame{
        panels.rightContent.x + margin + coverWidth + gap + infoWidth + gap,
        panels.rightContent.y + margin,
        sideWidth,
        (topHeight - margin) * 0.52f
    };
    const SDL_FRect inner{
        quickFrame.x + 5.0f,
        quickFrame.y + 28.0f,
        quickFrame.w - 10.0f,
        quickFrame.h - 33.0f
    };

    // Treat each complete visual control row as one click target.  This includes
    // the label, value field, arrow button and the empty padding around them.
    // It is intentionally based on the same geometry used by DrawLibraryTab so
    // windowed and fullscreen layouts behave identically.
    const SDL_FRect biosHit{
        inner.x + 4.0f, inner.y + 5.0f,
        inner.w - 8.0f, 49.0f
    };
    const SDL_FRect regionHit{
        inner.x + 4.0f, inner.y + 58.0f,
        inner.w - 8.0f, 49.0f
    };
    const SDL_FRect scanlinesHit{
        inner.x + 4.0f, inner.y + 112.0f,
        inner.w - 8.0f, 37.0f
    };

    const auto contains = [x, y](const SDL_FRect& rect)
    {
        return x >= rect.x && x < rect.x + rect.w &&
               y >= rect.y && y < rect.y + rect.h;
    };

    if (contains(biosHit))
    {
        settingsSelected_ = 3;
        ActivateSettingsSelection();
        return true;
    }

    if (contains(regionHit))
    {
        settingsSelected_ = 1;
        ActivateSettingsSelection();
        return true;
    }

    if (contains(scanlinesHit))
    {
        settingsSelected_ = 2;
        ActivateSettingsSelection();
        return true;
    }

    return false;
}

bool FrontendApp::TryLibraryFavoriteAt(float x, float y, bool activate)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
    const float margin = 12.0f;
    const float gap = 10.0f;
    const float topHeight = (std::max)(250.0f, panels.rightContent.h * 0.61f);
    const float sideWidth = (std::clamp)(panels.rightContent.w * 0.22f, 210.0f, 285.0f);
    const float infoWidth = (std::clamp)(panels.rightContent.w * 0.30f, 270.0f, 370.0f);
    const float coverWidth = panels.rightContent.w - infoWidth - sideWidth -
        margin * 2.0f - gap * 2.0f;
    const SDL_FRect sideFrame{
        panels.rightContent.x + margin + coverWidth + gap + infoWidth + gap,
        panels.rightContent.y + margin,
        sideWidth,
        topHeight - margin
    };
    const float quickHeight = sideFrame.h * 0.52f;
    const SDL_FRect favoritesFrame{
        sideFrame.x,
        sideFrame.y + quickHeight + gap,
        sideFrame.w,
        sideFrame.h - quickHeight - gap
    };
    const SDL_FRect inner{
        favoritesFrame.x + 5.0f,
        favoritesFrame.y + 28.0f,
        favoritesFrame.w - 10.0f,
        favoritesFrame.h - 33.0f
    };

    if (x < inner.x || x >= inner.x + inner.w ||
        y < inner.y || y >= inner.y + inner.h)
        return false;

    std::vector<const GameInfo*> favorites;
    for (const GameInfo& candidate : library_.Games())
        if (candidate.favorite) favorites.push_back(&candidate);
    std::stable_sort(favorites.begin(), favorites.end(), [](const GameInfo* a, const GameInfo* b)
    {
        return a->title < b->title;
    });

    const int maximumRows = (std::max)(1, static_cast<int>((inner.h - 12.0f) / 24.0f));
    const int row = static_cast<int>((y - (inner.y + 9.0f)) / 24.0f);
    const int shown = (std::min)(maximumRows, static_cast<int>(favorites.size()));
    if (row < 0 || row >= shown)
        return false;

    if (!collections_.SelectFilename(favorites[static_cast<std::size_t>(row)]->filename))
        return false;

    libraryDescriptionScroll_ = 0;
    redraw_ = true;
    if (activate)
        ActivateSelection();
    return true;
}

bool FrontendApp::TryEditLibraryDescriptionAt(float x, float y)
{
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const FrontendPanelLayout panels =
        FrontendPanels_Calculate(windowWidth, windowHeight);

    const float margin = 12.0f;
    const float gap = 12.0f;
    const float topHeight = (std::max)(220.0f, panels.rightContent.h * 0.61f);
    const SDL_FRect descriptionFrame{
        panels.rightContent.x + margin,
        panels.rightContent.y + topHeight + gap,
        panels.rightContent.w - margin * 2.0f,
        panels.rightContent.h - topHeight - gap - margin
    };

    if (x < descriptionFrame.x ||
        x >= descriptionFrame.x + descriptionFrame.w ||
        y < descriptionFrame.y ||
        y >= descriptionFrame.y + descriptionFrame.h)
        return false;

    EditLibraryDescription();
    return true;
}

void FrontendApp::MoveMetadataSelection(int direction)
{
    metadataSelected_ += direction;
    if (metadataSelected_ < 0) metadataSelected_ = MetadataFieldCount - 1;
    if (metadataSelected_ >= MetadataFieldCount) metadataSelected_ = 0;
    redraw_ = true;
}

std::string* FrontendApp::CurrentMetadataField()
{
    switch (metadataSelected_)
    {
    case 0: return &metadataWorkingCopy_.title;
    case 1: return &metadataWorkingCopy_.year;
    case 2: return &metadataWorkingCopy_.publisher;
    case 3: return &metadataWorkingCopy_.developer;
    case 4: return &metadataWorkingCopy_.genre;
    case 5: return &metadataWorkingCopy_.players;
    case 6: return &metadataWorkingCopy_.controls;
    case 7: return &metadataWorkingCopy_.voiceModule;
    case 8: return &metadataWorkingCopy_.videopacPlus;
    case 9: return &metadataWorkingCopy_.rating;
    case 10: return &metadataWorkingCopy_.shortDescription;
    case 11: return &metadataWorkingCopy_.description;
    case 12: return &metadataWorkingCopy_.trivia;
    case 13: return &metadataManualPath_;
    default: return nullptr;
    }
}

const char* FrontendApp::CurrentMetadataLabel() const
{
    static const char* labels[MetadataFieldCount] = {
        "Title", "Year", "Publisher", "Developer", "Genre", "Players",
        "Controls", "Voice Module", "Videopac+", "Rating", "Short Description",
        "Game Description", "Trivia / History", "Manual Path"
    };
    return labels[metadataSelected_];
}

void FrontendApp::BeginMetadataEdit()
{
    GameInfo* game = GetSelectedGame();
    if (!game || metadataEditMode_) return;
    metadataWorkingCopy_ = *game;
    metadataManualPath_ = game->manual.empty() ? std::string() : game->manual.string();
    metadataSelected_ = 0;
    metadataCaret_ = metadataWorkingCopy_.title.size();
    metadataEditMode_ = true;
    metadataTextInput_ = false;
    redraw_ = true;
}

void FrontendApp::CancelMetadataEdit()
{
    if (metadataTextInput_) SDL_StopTextInput(window_);
    metadataTextInput_ = false;
    metadataEditMode_ = false;
    redraw_ = true;
}

void FrontendApp::SaveMetadataEdit()
{
    GameInfo* game = GetSelectedGame();
    if (!game) return;
    if (metadataTextInput_) SDL_StopTextInput(window_);
    metadataTextInput_ = false;
    if (!metadataManualPath_.empty())
    {
        std::filesystem::path path(metadataManualPath_);
        metadataWorkingCopy_.manual = path.is_absolute() ? path : gameDatabase_.BasePath() / path;
    }
    else
        metadataWorkingCopy_.manual.clear();

    if (gameDatabase_.SaveUserMetadata(metadataWorkingCopy_))
    {
        *game = metadataWorkingCopy_;
        collections_.Rebuild();
        metadataEditMode_ = false;
        std::printf("O2EM-NG: metadata saved for %s.\n", game->filename.c_str());
    }
    redraw_ = true;
}

bool FrontendApp::IsLongMetadataField() const noexcept
{
    return metadataSelected_ >= 10 && metadataSelected_ <= 12;
}

void FrontendApp::EditCurrentMetadataField()
{
    if (!metadataEditMode_) return;

    std::string* field = CurrentMetadataField();
    if (!field) return;

    if (IsLongMetadataField())
    {
        if (metadataTextInput_)
        {
            SDL_StopTextInput(window_);
            metadataTextInput_ = false;
        }
        OpenMetadataTextEditor(CurrentMetadataLabel(), true, *field);
        redraw_ = true;
        return;
    }

    ToggleMetadataTextInput();
}

void FrontendApp::ToggleMetadataTextInput()
{
    if (!metadataEditMode_) return;
    metadataTextInput_ = !metadataTextInput_;
    if (metadataTextInput_)
    {
        if (std::string* field = CurrentMetadataField()) metadataCaret_ = field->size();
        SDL_StartTextInput(window_);
    }
    else SDL_StopTextInput(window_);
    redraw_ = true;
}

void FrontendApp::OpenSelectedManual()
{
    const GameInfo* game = GetSelectedGame();
    if (!game || game->manual.empty()) return;
    std::error_code error;
    if (!std::filesystem::is_regular_file(game->manual, error) || error) return;
    ShellExecuteW(nullptr, L"open", game->manual.wstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool FrontendApp::TryActivateManualAt(float x, float y)
{
    int w=0,h=0; SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w,h);
    const SDL_FRect r = panels.rightContent;
    if (x >= r.x+35 && x < r.x+r.w-35 && y >= r.y+105 && y < r.y+r.h-55)
    { OpenSelectedManual(); return true; }
    return false;
}

bool FrontendApp::TryMetadataControlAt(float x, float y)
{
    int w=0,h=0; SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w,h);
    const SDL_FRect c=panels.rightContent;
    if (!metadataEditMode_)
    {
        if (x>=c.x+c.w-190 && x<c.x+c.w-30 && y>=c.y+c.h-58 && y<c.y+c.h-24)
        { BeginMetadataEdit(); return true; }
        return false;
    }
    const float firstY=c.y+58.0f;
    for(int i=0;i<MetadataFieldCount;++i)
    {
        float ry=firstY+i*29.0f;
        if(x>=c.x+20 && x<c.x+c.w-20 && y>=ry-5 && y<ry+22)
        { metadataSelected_=i; redraw_=true; if(i!=13) EditCurrentMetadataField(); return true; }
    }
    if(y>=c.y+c.h-58 && y<c.y+c.h-24)
    {
        if(x>=c.x+25 && x<c.x+155) { SaveMetadataEdit(); return true; }
        if(x>=c.x+170 && x<c.x+300) { CancelMetadataEdit(); return true; }
    }
    return false;
}



void FrontendApp::RefreshInstalledBiosFiles(bool preserveSelection)
{
    installedBiosFiles_.clear();
    const std::filesystem::path folder = importManager_.BasePath() / "BIOS";
    std::error_code error;
    if (std::filesystem::is_directory(folder, error) && !error)
    {
        for (const auto& entry : std::filesystem::directory_iterator(folder, error))
        {
            if (error) break;
            if (!entry.is_regular_file(error) || error) { error.clear(); continue; }
            std::string extension = entry.path().extension().string();
            std::transform(extension.begin(), extension.end(), extension.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (extension == ".bin" || extension == ".rom")
                installedBiosFiles_.push_back(entry.path().filename().string());
        }
    }
    std::stable_sort(installedBiosFiles_.begin(), installedBiosFiles_.end(),
        [](const std::string& a, const std::string& b)
        {
            std::string lowerA = a, lowerB = b;
            std::transform(lowerA.begin(), lowerA.end(), lowerA.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            std::transform(lowerB.begin(), lowerB.end(), lowerB.begin(),
                [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return lowerA < lowerB;
        });

    const bool currentExists = std::find(installedBiosFiles_.begin(),
        installedBiosFiles_.end(), settings_.bios_file) != installedBiosFiles_.end();
    if (!preserveSelection || !currentExists)
        settings_.bios_file = installedBiosFiles_.empty() ? std::string() : installedBiosFiles_.front();
}

bool FrontendApp::HasInstalledBios() const noexcept
{
    return !installedBiosFiles_.empty();
}

bool FrontendApp::SelectedBiosExists() const noexcept
{
    return !settings_.bios_file.empty() &&
        std::find(installedBiosFiles_.begin(), installedBiosFiles_.end(),
            settings_.bios_file) != installedBiosFiles_.end();
}

void FrontendApp::RefreshSelectedGameAssets(GameInfo& game)
{
    // Patch 0022d: refresh the selected record and every media cache at once.
    // Previously the newly imported cover/manual became visible only after
    // changing selection (or restarting), because the current texture/path
    // caches still represented the old state.
    assetManager_.Populate(game);
    FrontendBoxArt_Shutdown();
    ManualPreview_Shutdown();
    FrontendScreenshot_Invalidate();
    collections_.Rebuild();
}

void FrontendApp::RunImport(ImportAssetType type)
{
    GameInfo* game = GetSelectedGame();
    if (type != ImportAssetType::Bios && !game)
    {
        importStatus_ = "Select a game before importing this file.";
        redraw_ = true;
        return;
    }

    // Patch 0022d revised: IMPORT always means import a ROM for the selected
    // catalogue title. If a ROM is already present, confirm before opening
    // the file dialogs so an installed game cannot be overwritten by mistake.
    if (type == ImportAssetType::Rom && game && !game->romPath.empty())
    {
        // SDL_Window* cannot be passed to the Win32 MessageBoxA(HWND, ...).
        // Use SDL3's native message-box API so the parent window type is correct
        // and the confirmation remains portable.
        const SDL_MessageBoxButtonData buttons[] =
        {
            { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 1, "Yes" },
            { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "No" }
        };

        const SDL_MessageBoxData messageBox =
        {
            SDL_MESSAGEBOX_WARNING,
            window_,
            "O2EM-NG - Confirm ROM import",
            "A ROM is already installed for this game.\n\n"
            "Do you want to import another ROM and overwrite the installed file?",
            static_cast<int>(std::size(buttons)),
            buttons,
            nullptr
        };

        int selectedButton = 0;
        if (!SDL_ShowMessageBox(&messageBox, &selectedButton) || selectedButton != 1)
        {
            importStatus_ = "ROM import cancelled.";
            redraw_ = true;
            return;
        }
    }

    const ImportResult result = (type == ImportAssetType::Bios)
        ? importManager_.ImportBios()
        : importManager_.ImportForGame(type, *game);
    importStatus_ = result.message;
    if (result.success)
    {
        if (type == ImportAssetType::Bios)
        {
            RefreshInstalledBiosFiles(false);
            settings_.bios_file = result.destination.filename().string();
            SaveSettings(settingsPath_, settings_);
        }
        if (type == ImportAssetType::Rom && game)
        {
            game->romPath = result.destination;
            // Keep the catalogue filename as the stable database key even if
            // the user changes the destination name in the save dialog.
            game->rom.path = result.destination.string();
        }
        if (game)
            RefreshSelectedGameAssets(*game);
        std::printf("O2EM-NG: %s\n", result.message.c_str());
    }
    redraw_ = true;
}

void FrontendApp::RunDelete(ImportAssetType type)
{
    GameInfo* game = GetSelectedGame();
    if (type != ImportAssetType::Bios && !game)
    {
        importStatus_ = "Select a game before deleting this file.";
        redraw_ = true;
        return;
    }

    const std::filesystem::path screenshotPath =
        type == ImportAssetType::Screenshot
        ? FrontendScreenshot_CurrentPath(game)
        : std::filesystem::path{};

    const ImportResult result = type == ImportAssetType::Bios
        ? importManager_.DeleteBios()
        : importManager_.DeleteForGame(type, *game, screenshotPath);
    importStatus_ = result.message;

    if (result.success)
    {
        if (type == ImportAssetType::Bios)
        {
            RefreshInstalledBiosFiles();
            SaveSettings(settingsPath_, settings_);
        }
        else if (game)
        {
            if (type == ImportAssetType::Rom)
            {
                game->romPath.clear();
                // Keep filename: it identifies the catalogue/database record and
                // allows the ROM to be imported again after deletion.
                game->rom.path.clear();
            }
            RefreshSelectedGameAssets(*game);
        }
        std::printf("O2EM-NG: %s\n", result.message.c_str());
    }
    redraw_ = true;
}

bool FrontendApp::TryImportControlAt(float x, float y)
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window_, &width, &height);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
    const SDL_FRect content = panels.rightContent;
    const GameInfo* game = GetSelectedGame();

    const float rowX = content.x + 48.0f;
    const float labelWidth = 150.0f;
    const float statusWidth = 160.0f;
    const float addWidth = 120.0f;
    const float deleteWidth = 120.0f;
    const float columnGap = 12.0f;
    const float firstY = content.y + 158.0f;
    const float buttonHeight = 38.0f;
    const float gap = 12.0f;
    const float addX = rowX + labelWidth + statusWidth;

    for (int index = 0; index < 5; ++index)
    {
        const ImportAssetType type = static_cast<ImportAssetType>(index);
        const float rowY = firstY + static_cast<float>(index) * (buttonHeight + gap);
        const SDL_FRect addButton{addX, rowY, addWidth, buttonHeight};
        const SDL_FRect deleteButton{
            addX + addWidth + columnGap, rowY, deleteWidth, buttonHeight};

        if (x >= addButton.x && x < addButton.x + addButton.w &&
            y >= addButton.y && y < addButton.y + addButton.h)
        {
            RunImport(type);
            return true;
        }

        bool canDelete = type == ImportAssetType::Bios && HasInstalledBios();
        if (game)
        {
            switch (type)
            {
            case ImportAssetType::Rom: canDelete = !game->romPath.empty(); break;
            case ImportAssetType::Manual: canDelete = !game->manual.empty(); break;
            case ImportAssetType::Cover: canDelete = !game->boxArt.empty(); break;
            case ImportAssetType::Screenshot: canDelete = !game->screenshots.empty(); break;
            case ImportAssetType::Bios: break;
            }
        }

        if (canDelete && x >= deleteButton.x && x < deleteButton.x + deleteButton.w &&
            y >= deleteButton.y && y < deleteButton.y + deleteButton.h)
        {
            RunDelete(type);
            return true;
        }
    }
    return false;
}

void FrontendApp::DrawImportCenter(const SDL_FRect& content)
{
    const GameInfo* game = GetSelectedGame();
    const float margin = 14.0f;
    const SDL_FRect frame{
        content.x + margin,
        content.y + margin,
        content.w - margin * 2.0f,
        content.h - margin * 2.0f
    };
    DrawSunkenFrame(renderer_, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

    DrawText(renderer_, inner.x + 20.0f, inner.y + 18.0f, 1.55f, "IMPORT CENTER");
    DrawText(renderer_, inner.x + 20.0f, inner.y + 56.0f, 1.05f,
        game ? ("Selected game: " + game->title) : "Select a game in the library first.");
    DrawText(renderer_, inner.x + 20.0f, inner.y + 82.0f, 0.98f,
        "Add files or safely move existing files to the Recycle Bin.");

    if (game)
    {
        const std::string mediaStatus =
            std::string("MEDIA STATUS   ROM: ") +
            (!game->romPath.empty() ? "YES" : "NO") +
            "   COVER: " + (!game->boxArt.empty() ? "YES" : "NO") +
            "   MANUAL: " + (!game->manual.empty() ? "YES" : "NO") +
            "   SCREENSHOTS: " + std::to_string(game->screenshots.size());
        DrawText(renderer_, inner.x + 20.0f, inner.y + 108.0f, 0.98f, mediaStatus);
    }

    const char* rowLabels[5] = { "ROM", "BIOS", "MANUAL", "COVER", "SCREENSHOTS" };
    const char* addLabels[5] = { "ADD", "ADD", "ADD", "ADD", "ADD" };
    const float rowX = content.x + 48.0f;
    const float labelWidth = 150.0f;
    const float statusWidth = 160.0f;
    const float addWidth = 120.0f;
    const float deleteWidth = 120.0f;
    const float columnGap = 12.0f;
    const float firstY = content.y + 158.0f;
    const float buttonHeight = 38.0f;
    const float gap = 12.0f;
    const float addX = rowX + labelWidth + statusWidth;

    auto drawButton = [&](const SDL_FRect& button, const char* label, bool enabled)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &button);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, button.x, button.y,
            button.x + button.w - 1.0f, button.y);
        SDL_RenderLine(renderer_, button.x, button.y,
            button.x, button.y + button.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderLine(renderer_, button.x, button.y + button.h - 1.0f,
            button.x + button.w - 1.0f, button.y + button.h - 1.0f);
        SDL_RenderLine(renderer_, button.x + button.w - 1.0f, button.y,
            button.x + button.w - 1.0f, button.y + button.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_,
            enabled ? Win95Theme::WindowText : Win95Theme::Shadow);
        DrawText(renderer_, button.x + 29.0f, button.y + 7.0f, 1.0f, label);
    };

    for (int index = 0; index < 5; ++index)
    {
        const ImportAssetType type = static_cast<ImportAssetType>(index);
        const float rowY = firstY + static_cast<float>(index) * (buttonHeight + gap);
        const SDL_FRect addButton{addX, rowY, addWidth, buttonHeight};
        const SDL_FRect deleteButton{
            addX + addWidth + columnGap, rowY, deleteWidth, buttonHeight};

        bool canDelete = type == ImportAssetType::Bios && HasInstalledBios();
        if (game)
        {
            switch (type)
            {
            case ImportAssetType::Rom: canDelete = !game->romPath.empty(); break;
            case ImportAssetType::Manual: canDelete = !game->manual.empty(); break;
            case ImportAssetType::Cover: canDelete = !game->boxArt.empty(); break;
            case ImportAssetType::Screenshot: canDelete = !game->screenshots.empty(); break;
            case ImportAssetType::Bios: break;
            }
        }

        std::string statusText = "Not installed";
        if (type == ImportAssetType::Bios)
            statusText = installedBiosFiles_.empty()
                ? "None installed"
                : std::to_string(installedBiosFiles_.size()) +
                    (installedBiosFiles_.size() == 1 ? " file" : " files");
        else if (game)
        {
            switch (type)
            {
            case ImportAssetType::Rom: statusText = game->romPath.empty() ? "Missing" : "Present"; break;
            case ImportAssetType::Manual: statusText = game->manual.empty() ? "Missing" : "Present"; break;
            case ImportAssetType::Cover: statusText = game->boxArt.empty() ? "Missing" : "Present"; break;
            case ImportAssetType::Screenshot:
                statusText = std::to_string(game->screenshots.size()) +
                    (game->screenshots.size() == 1 ? " file" : " files");
                break;
            case ImportAssetType::Bios: break;
            }
        }

        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, rowX, rowY + 7.0f, 1.05f, rowLabels[index]);
        DrawText(renderer_, rowX + labelWidth, rowY + 7.0f, 1.0f, statusText);
        const char* addLabel = addLabels[index];
        if (type == ImportAssetType::Rom)
            addLabel = "IMPORT";
        drawButton(addButton, addLabel, true);
        drawButton(deleteButton, "DELETE", canDelete);
    }

    const std::string status = importStatus_.empty()
        ? "Delete moves files to the Recycle Bin. Screenshot deletes the image currently displayed."
        : importStatus_;
    DrawText(renderer_, inner.x + 20.0f, inner.y + inner.h - 50.0f, 0.95f, status);
}

void FrontendApp::DrawGameInformationTab(const SDL_FRect& content)
{
    const float margin = 14.0f;
    const SDL_FRect frame{
        content.x + margin,
        content.y + margin,
        content.w - margin * 2.0f,
        content.h - margin * 2.0f
    };
    DrawSunkenFrame(renderer_, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);

    const GameInfo* source = metadataEditMode_
        ? &metadataWorkingCopy_
        : GetSelectedGame();
    if (!source)
        return;

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(
        renderer_,
        inner.x + 20.0f,
        inner.y + 16.0f,
        1.6f,
        metadataEditMode_ ? "EDIT GAME DATA" : "GAME DATA");

    const std::string idText = source->videopacNumber > 0
        ? (source->videopacNumber < 10
            ? "0" + std::to_string(source->videopacNumber)
            : std::to_string(source->videopacNumber))
        : "-";

    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + 48.0f,
        1.1f,
        "Videopac No.: " + idText);
    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + 69.0f,
        1.0f,
        "ROM File:     " + source->filename);

    const std::string values[MetadataFieldCount] = {
        source->title,
        source->year,
        source->publisher,
        source->developer,
        source->genre,
        source->players,
        source->controls,
        source->voiceModule,
        source->videopacPlus,
        source->rating,
        source->shortDescription,
        source->description,
        source->trivia,
        metadataEditMode_ ? metadataManualPath_ : source->manual.string()
    };

    static const char* labels[MetadataFieldCount] = {
        "Title", "Year", "Publisher", "Developer", "Genre", "Players",
        "Controls", "Voice Module", "Videopac+", "Rating",
        "Short Description", "Game Description", "Trivia / History", "Manual Path"
    };

    const float footerSpace = 48.0f;
    const float firstRowY = inner.y + 98.0f;
    const float availableHeight = inner.h - (firstRowY - inner.y) - footerSpace;
    const float rowStep = (std::max)(18.0f,
        (std::min)(29.0f, availableHeight / static_cast<float>(MetadataFieldCount)));

    float y = firstRowY;
    for (int index = 0; index < MetadataFieldCount; ++index)
    {
        if (metadataEditMode_ && index == metadataSelected_)
        {
            SDL_SetRenderDrawColor(renderer_, 185, 35, 35, 255);
            const SDL_FRect highlight{
                inner.x + 14.0f,
                y - 4.0f,
                inner.w - 28.0f,
                rowStep - 1.0f
            };
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        }

        std::string value = values[index].empty() ? "-" : values[index];
        const std::size_t maximumLength = inner.w > 650.0f ? 82u : 58u;
        if (value.size() > maximumLength)
            value = value.substr(0, maximumLength - 3) + "...";

        const float rowScale = rowStep < 22.0f ? 0.9f : 1.0f;
        DrawText(
            renderer_,
            inner.x + 24.0f,
            y,
            rowScale,
            std::string(labels[index]) + ": " + value);

        if (metadataTextInput_ && index == metadataSelected_)
        {
            const std::size_t shownCaret = (std::min)(metadataCaret_, value.size());
            const std::size_t prefixCharacters = std::string(labels[index]).size() + 2u;
            const float caretX = inner.x + 24.0f +
                8.0f * rowScale * static_cast<float>(prefixCharacters + shownCaret);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
            SDL_RenderLine(renderer_, caretX, y - 2.0f, caretX, y + 15.0f * rowScale);
            SDL_RenderLine(renderer_, caretX + 1.0f, y - 2.0f, caretX + 1.0f, y + 15.0f * rowScale);
        }
        y += rowStep;
    }

    if (metadataEditMode_)
    {
        DrawText(
            renderer_,
            inner.x + 24.0f,
            inner.y + inner.h - 38.0f,
            1.1f,
            "[ SAVE ]   [ CANCEL ]   ENTER: edit field   Description/Trivia: full editor");
        if (metadataTextInput_)
        {
            DrawText(
                renderer_,
                inner.x + inner.w - 185.0f,
                inner.y + 18.0f,
                1.0f,
                "TYPING...");
        }
    }
    else
    {
        DrawText(
            renderer_,
            inner.x + inner.w - 175.0f,
            inner.y + inner.h - 38.0f,
            1.1f,
            "[ EDIT GAME DATA ]");
    }
}

void FrontendApp::DrawManualTab(const SDL_FRect& content)
{
    const float margin = 14.0f;
    const SDL_FRect frame{
        content.x + margin,
        content.y + margin,
        content.w - margin * 2.0f,
        content.h - margin * 2.0f
    };
    DrawSunkenFrame(renderer_, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

    DrawText(renderer_, inner.x + 24.0f, inner.y + 18.0f, 1.7f, "MANUAL");

    const GameInfo* game = GetSelectedGame();
    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + 62.0f,
        1.25f,
        game ? "Selected game: " + game->title : "No game selected");

    if (game && !game->manual.empty() && std::filesystem::exists(game->manual))
    {
        const SDL_FRect previewFrame{
            inner.x + 36.0f,
            inner.y + 96.0f,
            inner.w - 72.0f,
            inner.h - 170.0f
        };
        DrawSunkenFrame(renderer_, previewFrame);

        const SDL_FRect previewArea{
            previewFrame.x + 8.0f,
            previewFrame.y + 8.0f,
            previewFrame.w - 16.0f,
            previewFrame.h - 16.0f
        };
        SDL_SetRenderDrawColor(renderer_, 245, 245, 245, 255);
        SDL_RenderFillRect(renderer_, &previewArea);

        if (!ManualPreview_Draw(renderer_, game->manual, previewArea))
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
            DrawText(
                renderer_,
                previewArea.x + 24.0f,
                previewArea.y + previewArea.h * 0.5f - 24.0f,
                1.25f,
                "FIRST-PAGE PREVIEW NOT AVAILABLE");
            DrawText(
                renderer_,
                previewArea.x + 24.0f,
                previewArea.y + previewArea.h * 0.5f + 8.0f,
                1.0f,
                "The PDF can still be opened normally.");
        }

        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(
            renderer_,
            inner.x + 24.0f,
            inner.y + inner.h - 50.0f,
            1.05f,
            game->manual.filename().string());
        DrawText(
            renderer_,
            inner.x + 24.0f,
            inner.y + inner.h - 28.0f,
            1.05f,
            "Click the preview or press Enter/A to open the complete PDF.");
    }
    else
    {
        DrawText(renderer_, inner.x + 40.0f, inner.y + 145.0f, 1.5f, "NO MANUAL AVAILABLE");
        DrawText(
            renderer_,
            inner.x + 40.0f,
            inner.y + 190.0f,
            1.1f,
            "Set Manual Path under Game Information > Edit Metadata.");
    }
}

void FrontendApp::DrawAboutTab(const SDL_FRect& content)
{
    DrawProjectPage(content);
}

void FrontendApp::DrawCreditsTab(const SDL_FRect& content)
{
    DrawProjectPage(content);
}

void FrontendApp::DrawProjectPage(const SDL_FRect& content)
{
    const float margin = 14.0f;
    const SDL_FRect frame{content.x + margin, content.y + margin,
        content.w - margin * 2.0f, content.h - margin * 2.0f};
    DrawSunkenFrame(renderer_, frame);
    const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f, frame.w - 8.0f, frame.h - 8.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);

    if (projectPages_.empty())
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, inner.x + 24.0f, inner.y + 24.0f, 1.2f, "Project information is not available.");
        return;
    }
    projectPageIndex_ = (std::max)(0, (std::min)(projectPageIndex_, static_cast<int>(projectPages_.size()) - 1));
    const ProjectPage& page = projectPages_[projectPageIndex_];

    const float buttonGap = 4.0f;
    const float buttonWidth = (inner.w - 24.0f - buttonGap * 5.0f) / 6.0f;
    for (int i = 0; i < static_cast<int>(projectPages_.size()) && i < 6; ++i)
    {
        SDL_FRect button{inner.x + 12.0f + i * (buttonWidth + buttonGap), inner.y + 12.0f, buttonWidth, 30.0f};
        Win95Theme::SetRenderColor(renderer_, i == projectPageIndex_ ? Win95Theme::TabActive : Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &button);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderRect(renderer_, &button);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, button.x + 7.0f, button.y + 7.0f, 0.72f, projectPages_[i].title);
    }

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, inner.x + 22.0f, inner.y + 60.0f, 1.55f, page.title);
    DrawText(renderer_, inner.x + 22.0f, inner.y + 94.0f, 0.9f,
        "Editable project information stored in GAMEDATA/o2em-ng.db");

    SDL_FRect editButton{inner.x + inner.w - 118.0f, inner.y + 57.0f, 92.0f, 30.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &editButton);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
    SDL_RenderRect(renderer_, &editButton);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, editButton.x + 20.0f, editButton.y + 7.0f, 0.9f, "Edit...");

    SDL_FRect textFrame{inner.x + 18.0f, inner.y + 126.0f, inner.w - 36.0f, inner.h - 150.0f};
    DrawSunkenFrame(renderer_, textFrame);
    SDL_FRect textArea{textFrame.x + 8.0f, textFrame.y + 8.0f, textFrame.w - 22.0f, textFrame.h - 16.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &textArea);
    const int maxCharacters = (std::max)(20, static_cast<int>(textArea.w / 9.0f));
    const std::vector<std::string> lines = WrapProjectText(page.content, maxCharacters);
    const int visibleLines = (std::max)(1, static_cast<int>(textArea.h / 23.0f));
    const int maxScroll = (std::max)(0, static_cast<int>(lines.size()) - visibleLines);
    projectPageScroll_ = (std::min)(projectPageScroll_, maxScroll);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    float y = textArea.y + 4.0f;
    for (int i = projectPageScroll_; i < static_cast<int>(lines.size()) && i < projectPageScroll_ + visibleLines; ++i)
    {
        DrawText(renderer_, textArea.x + 4.0f, y, 0.92f, lines[i]);
        y += 23.0f;
    }
    if (maxScroll > 0)
    {
        SDL_FRect track{textFrame.x + textFrame.w - 12.0f, textArea.y, 6.0f, textArea.h};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face); SDL_RenderFillRect(renderer_, &track);
        const float thumbHeight = (std::max)(18.0f, track.h * visibleLines / static_cast<float>(lines.size()));
        const float thumbY = track.y + (track.h - thumbHeight) * projectPageScroll_ / static_cast<float>(maxScroll);
        SDL_FRect thumb{track.x, thumbY, track.w, thumbHeight};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow); SDL_RenderFillRect(renderer_, &thumb);
    }
}

void FrontendApp::DrawSettingsTab(const SDL_FRect& content)
{
    const float margin = 14.0f;
    const SDL_FRect frame{
        content.x + margin,
        content.y + margin,
        content.w - margin * 2.0f,
        content.h - margin * 2.0f
    };

    DrawSunkenFrame(renderer_, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };

    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

    DrawText(
        renderer_,
        inner.x + 20.0f,
        inner.y + 18.0f,
        1.5f,
        "EMULATOR SETTINGS");

    const float rowY[SettingsItemCount] = {
        inner.y + 75.0f,
        inner.y + 135.0f,
        inner.y + 195.0f,
        inner.y + 255.0f
    };

    for (int index = 0; index < SettingsItemCount; ++index)
    {
        if (index == settingsSelected_)
        {
            SDL_SetRenderDrawColor(renderer_, 185, 35, 35, 255);

            const SDL_FRect highlight{
                inner.x + 18.0f,
                rowY[index] - 12.0f,
                inner.w - 36.0f,
                42.0f
            };

            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        }

        std::string line;

        switch (index)
        {
        case 0:
            line = std::string("Fullscreen: ") +
                (settings_.start_fullscreen ? "ON" : "OFF") +
                "  (changes immediately)";
            break;

        case 1:
            line = std::string("Region Mode: ") +
                RegionModeToString(settings_.region_mode);
            break;

        case 2:
            line = std::string("Scanlines: ") +
                (settings_.scanlines ? "ON" : "OFF");
            break;

        case 3:
            line = std::string("BIOS File: ") + settings_.bios_file;
            break;

        default:
            break;
        }

        DrawText(renderer_, inner.x + 36.0f, rowY[index], 1.6f, line);
    }

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + inner.h - 42.0f,
        1.2f,
        "UP/DOWN or mouse: choose   ENTER/A/click: change   ESC/B: Library");
}

void FrontendApp::DrawPlaceholderTab(
    const SDL_FRect& content,
    const char* title,
    const char* message)
{
    const float margin = 14.0f;
    const SDL_FRect frame{
        content.x + margin,
        content.y + margin,
        content.w - margin * 2.0f,
        content.h - margin * 2.0f
    };

    DrawSunkenFrame(renderer_, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };

    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    SDL_RenderFillRect(renderer_, &inner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

    DrawText(renderer_, inner.x + 24.0f, inner.y + 22.0f, 1.7f, title);

    const GameInfo* game = GetSelectedGame();
    const std::string selectedGame =
        game ? "Selected game: " + game->title : "No game selected";

    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + 82.0f,
        1.35f,
        selectedGame);

    DrawText(
        renderer_,
        inner.x + 24.0f,
        inner.y + 125.0f,
        1.25f,
        message ? message : "");
}
