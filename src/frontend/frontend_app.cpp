#include "src/frontend/frontend_app.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <ctime>
#include <cmath>
#include <string>
#include <vector>
#include <filesystem>
#include <sstream>

#define NOMINMAX
#include <windows.h>
#include <shellapi.h>

#include "frontend_boxart.h"
#include "src/version.h"
#include "src/frontend/frontend_screenshot.h"
#include "frontend_layout.h"
#include "frontend_panels.h"
#include "frontend_statusbar.h"
#include "input_manager.h"
#include "metadata_text_editor.h"
#include "launcher.h"
#include "src/database/library_view.h"
#include "src/media/manual_preview.h"
#include "src/update_check.h"
#include "theme_win95.h"
#include "ui_font.h"
#include "videopac_font.h"
#include "vdc_stub.h"

namespace
{
    // Width at which the far-right folders panel switches to its compact
    // presentation (short labels, no footer note). Below the existing 170px
    // column threshold the panel is folded away entirely.
    constexpr float kFoldersComfortWidth = 200.0f;
    struct DashboardLayout
    {
        SDL_FRect cover, info, description, quick, system, imports, recent,
            favorites, welcome;
        // Far-right column (present only when the media panel's aspect cap
        // leaves unused space right of Emulator Settings): Folders on top,
        // Collection Statistics in the middle and the My Collection summary at
        // the bottom, matching the approved Library design.
        SDL_FRect folders{}, stats{}, collection{};
        bool hasRightColumn = false;
        // Minimum-content fallback state: the column exists but is narrow.
        bool compactFolders = false;
    };
    DashboardLayout Dashboard(const SDL_FRect& c)
    {
        const float gap=10, margin=12;
        // Left/center top area (Cover / Game Information / Description).
        const float topH=c.h-190;
        const float sideW=(std::clamp)(c.w*0.22f,210.0f,285.0f);
        const float infoW=(std::clamp)(c.w*0.32f,270.0f,400.0f);
        const float coverW=(std::min)(c.w-infoW-sideW-44,(topH-33)*0.75f+10);
        DashboardLayout d{};
        d.cover={c.x+margin,c.y+margin,coverW,topH};
        d.info={d.cover.x+coverW+gap,d.cover.y,infoW,360};
        d.description={d.info.x,d.info.y+370,infoW,topH-370};

        // Right-hand side: a compact 2x2 grid on top, then a TALL My
        // Collection panel that fills the entire lower-right area down to the
        // same baseline as the bottom strip (no dead space).
        const float right1X=d.info.x+infoW+gap;        // left sub-column
        const float rightEdge=c.x+c.w-margin;
        const float extraRight=rightEdge-(right1X+sideW);
        const float bottomY=c.y+c.h-166;
        const float bottomBottom=c.y+c.h-12;
        const float bottomH=bottomBottom-bottomY;

        if(extraRight>=170.0f)
        {
            d.hasRightColumn=true;
            d.compactFolders=extraRight<kFoldersComfortWidth;
            const float farW=extraRight-gap;
            const float right2X=right1X+sideW+gap;      // right sub-column
            const float rightSideW=rightEdge-right1X;

            // Compact upper grid: fully reclaims the height for My Collection.
            const float row1H=195.0f;                   // Emulator Settings | Folders
            const float row2H=198.0f;                   // System Information | Statistics
            d.quick  ={right1X,d.cover.y,sideW,row1H};
            d.folders={right2X,d.cover.y,farW,row1H};
            const float row2Top=d.cover.y+row1H+gap;
            d.system={right1X,row2Top,sideW,row2H};
            d.stats ={right2X,row2Top,farW,row2H};

            const float collectionTop=row2Top+row2H+gap;
            d.collection={right1X,collectionTop,rightSideW,
                (std::max)(140.0f,bottomBottom-collectionTop)};
            d.welcome=d.collection;   // alias for any legacy caller

            // Bottom strip stops before My Collection: Quick Add / Recently
            // Played / Favorites only span the left-hand columns.
            const float bottomLeft=c.x+margin;
            const float bottomRight=right1X-gap;
            const float bottomW=(std::max)(200.0f,bottomRight-bottomLeft);
            const float column=(bottomW-gap*2.0f)/3.0f;
            d.imports={bottomLeft,bottomY,column,bottomH};
            d.recent={d.imports.x+column+gap,bottomY,column,bottomH};
            d.favorites={d.recent.x+column+gap,bottomY,column,bottomH};
        }
        else
        {
            // No right column: keep a compact Emulator Settings / System
            // Information stack and a four-panel bottom strip.
            d.quick={right1X,d.cover.y,sideW,220};
            d.system={right1X,d.quick.y+230,sideW,topH-230};
            const float column=(c.w-24-30)/4;
            d.imports={c.x+margin,bottomY,column,bottomH};
            d.recent={d.imports.x+column+gap,bottomY,column,bottomH};
            d.favorites={d.recent.x+column+gap,bottomY,column,bottomH};
            d.welcome={d.favorites.x+column+gap,bottomY,column,bottomH};
            d.collection=d.welcome;
        }
        return d;
    }

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


    std::string DisplayCatalogId(const GameInfo* game)
    {
        if (!game) return {};
        if (!game->catalogId.empty()) return game->catalogId;
        if (game->videopacNumber > 0)
        {
            char buffer[16]{};
            std::snprintf(buffer, sizeof(buffer), "%02d", game->videopacNumber);
            return buffer;
        }
        return {};
    }


    std::string TrimCatalogId(std::string value)
    {
        const auto first = std::find_if_not(value.begin(), value.end(),
            [](unsigned char c) { return std::isspace(c) != 0; });
        const auto last = std::find_if_not(value.rbegin(), value.rend(),
            [](unsigned char c) { return std::isspace(c) != 0; }).base();
        if (first >= last) return {};
        return std::string(first, last);
    }

    bool IsSafeCatalogId(const std::string& id)
    {
        if (id.empty()) return false;
        for (unsigned char c : id)
        {
            if (!(std::isalnum(c) || c == '+' || c == '-' || c == '_'))
                return false;
        }
        return true;
    }

    // Shared Win95 vertical scrollbar renderer (Favorites + Game Library):
    // raised arrow buttons, sunken track, raised proportional thumb.
    void DrawWin95VScrollbar(SDL_Renderer* renderer, const SDL_FRect& upArrow,
        const SDL_FRect& downArrow, const SDL_FRect& track, const SDL_FRect& thumb)
    {
        const auto drawArrowButton = [&](const SDL_FRect& rect, bool up)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
            SDL_RenderFillRect(renderer, &rect);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
            const float cx = rect.x + rect.w * 0.5f;
            const float cy = up ? rect.y + rect.h * 0.5f - 1.5f
                                : rect.y + rect.h * 0.5f + 1.5f;
            for (int k = 0; k < 4; ++k)
            {
                const float half = static_cast<float>(k);
                const float yy = up ? cy - 1.5f + static_cast<float>(k)
                                    : cy + 1.5f - static_cast<float>(k);
                SDL_RenderLine(renderer, cx - half, yy, cx + half, yy);
            }
        };
        drawArrowButton(upArrow, true);
        drawArrowButton(downArrow, false);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &track);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, track.x, track.y,
            track.x, track.y + track.h - 1.0f);
        SDL_RenderLine(renderer, track.x, track.y,
            track.x + track.w - 1.0f, track.y);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, track.x + track.w - 1.0f, track.y,
            track.x + track.w - 1.0f, track.y + track.h - 1.0f);
        SDL_RenderLine(renderer, track.x, track.y + track.h - 1.0f,
            track.x + track.w - 1.0f, track.y + track.h - 1.0f);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &thumb);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, thumb.x, thumb.y,
            thumb.x + thumb.w - 1.0f, thumb.y);
        SDL_RenderLine(renderer, thumb.x, thumb.y,
            thumb.x, thumb.y + thumb.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, thumb.x, thumb.y + thumb.h - 1.0f,
            thumb.x + thumb.w - 1.0f, thumb.y + thumb.h - 1.0f);
        SDL_RenderLine(renderer, thumb.x + thumb.w - 1.0f, thumb.y,
            thumb.x + thumb.w - 1.0f, thumb.y + thumb.h - 1.0f);
    }

    std::string CanonicalRomFilenameForCatalogId(const std::string& rawId)
    {
        const std::string id = TrimCatalogId(rawId);
        if (!IsSafeCatalogId(id)) return {};

        std::size_t digitCount = 0;
        while (digitCount < id.size() &&
            std::isdigit(static_cast<unsigned char>(id[digitCount])))
            ++digitCount;

        if (digitCount > 0)
        {
            const std::string suffix = id.substr(digitCount);
            if (suffix.empty() || suffix == "+")
            {
                const int number = std::stoi(id.substr(0, digitCount));
                if (number >= 1 && number <= 99)
                {
                    char buffer[32]{};
                    std::snprintf(buffer, sizeof(buffer), "vp_%02d%s.bin",
                        number, suffix.c_str());
                    return buffer;
                }
            }
        }

        return "vp_" + id + ".bin";
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
    // updateCheck_ joins its worker in its own destructor (member teardown).
    FrontendBoxArt_Shutdown();
    FrontendScreenshot_Shutdown();
    ManualPreview_Shutdown();
    FrontendLayout_Shutdown();
    FrontendChrome::Shutdown();
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
    // Patch 0024: the library is universal. Official catalogue entries are
    // added by the database below, but every installed .bin/.rom is visible.
    library_.SetGames(allRoms);

    // Resolve local assets and optional per-game metadata first. The database
    // is loaded last so user-owned edits always take precedence.
    assetManager_.SetBasePath(baseFolder);
    importManager_.SetBasePath(baseFolder);
    RefreshInstalledBiosFiles();

    // Patch 0030C Phase 4B: metadata/database identity must be loaded before
    // media is resolved. User Catalog IDs such as 54+, C7010 or Tutankham+
    // are required to find imported covers/manuals/screenshots reliably.
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

    // Resolve media last, after the database has restored user Catalog IDs.
    // This prevents media from disappearing on startup and then suddenly
    // reappearing after importing another screenshot/cover.
    assetManager_.Populate(library_);

    std::size_t manualCount = 0;
    for (const GameInfo& game : library_.Games())
    {
        if (!game.manual.empty())
            ++manualCount;

        std::printf(
            "O2EM-NG: media key %s | ROM: %s | Cover: %s | Manual: %s | Screenshots: %zu\n",
            game.catalogId.empty() ? "(fallback)" : game.catalogId.c_str(),
            game.filename.c_str(),
            game.boxArt.empty() ? "NOT FOUND" : game.boxArt.filename().string().c_str(),
            game.manual.empty() ? "NOT FOUND" : game.manual.filename().string().c_str(),
            game.screenshots.size());
    }
    std::printf(
        "O2EM-NG: resolved manuals for %zu of %zu games after database identity load.\n",
        manualCount, library_.Count());

    collections_.Attach(&library_);

    // My Collection: fully owned by CollectionPage. It creates and reads its
    // own database GAMEDATA/mycollection.db and only reads the main library
    // for reference metadata / cover reuse. The main DB is never modified.
    collectionPage_.Initialize(baseFolder, &library_, &gameDatabase_, window_);

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

    case SDL_EVENT_MOUSE_BUTTON_UP:
        HandleMouseButtonUp(event.button);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        HandleMouseMotion(event.motion);
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
    // The screenshot/video engine animates while the Library viewer shows the
    // media mode; it also self-invalidates whenever it is hidden.
    if (FrontendScreenshot_Update(GetSelectedGame(),
            activeTab_ == FrontendTab::Library &&
            libraryMediaMode_ == LibraryMediaMode::Screenshots))
        redraw_ = true;
    // Redraw when the background update check changes state (Checking ->
    // Up to date / New version available / failure) so the status line
    // updates without waiting for other input.
    {
        const UpdateCheck::State state = updateCheck_.GetState();
        if (state != updateCheckLastSeenState_)
        {
            updateCheckLastSeenState_ = state;
            redraw_ = true;
            switch (state)
            {
            case UpdateCheck::State::Checking:
                std::printf("O2EM-NG: update check started.\n");
                break;
            case UpdateCheck::State::UpToDate:
                std::printf("O2EM-NG: update check result: up to date (%s).\n",
                    O2emVersion::kAppVersion);
                break;
            case UpdateCheck::State::UpdateAvailable:
                std::printf("O2EM-NG: update check result: new version available (%s).\n",
                    updateCheck_.GetLatestVersion().c_str());
                break;
            case UpdateCheck::State::Error:
                std::printf("O2EM-NG: update check failed: %s\n",
                    updateCheck_.GetErrorMessage().c_str());
                break;
            default:
                break;
            }
        }
    }
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
    KeepLibrarySelectionVisible();
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
    KeepLibrarySelectionVisible();
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
    // My Collection is a fully separate page, not an About project page. It
    // reloads its own database so the table always reflects disk state.
    if (tab == FrontendTab::MyCollection) collectionPage_.Refresh();
    else collectionPage_.Deactivate(window_);
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

    if (activeTab_ == FrontendTab::About)
    {
        EditCurrentProjectPage();
        return;
    }

    // Enter/A on My Collection edits the selected entry (Phase B).
    if (activeTab_ == FrontendTab::MyCollection)
    {
        collectionPage_.ActivateSelected();
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

    // Catalogue entries can exist without their ROM (red number in the
    // library). They stay selectable for metadata/media, but there is
    // nothing to launch until the ROM is imported in Import Center.
    if (game->romPath.empty() && game->rom.path.empty())
    {
        const std::string message = "The ROM for \"" + game->title +
            "\" is not installed.\n\n"
            "Use Import Center > ROM to install it.";
        HWND owner = nullptr;
        if (window_)
        {
            owner = static_cast<HWND>(SDL_GetPointerProperty(
                SDL_GetWindowProperties(window_),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
        }
        MessageBoxA(owner, message.c_str(), "O2EM-NG - ROM not installed",
            MB_OK | MB_ICONINFORMATION);
        redraw_ = true;
        return;
    }

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

    std::printf("O2EM-NG: Launch request using BIOS: %s\n", settings_.bios_file.c_str());
    std::fflush(stdout);
    FrontendScreenshot_Invalidate(); // Release preview decoding during gameplay.
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

            std::printf("O2EM-NG: BIOS selected in frontend: %s\n", settings_.bios_file.c_str());
            std::fflush(stdout);
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

    // My Collection owns its own keyboard handling: search editing, row
    // navigation, dialogs. Keys it declines (Escape with nothing open,
    // Tab, Left/Right, shortcuts) fall through to the global handling.
    // The page edits state in place, so request a repaint whenever it
    // consumes a key (otherwise in-place edits such as Backspace would not
    // be shown until some other event triggered a redraw).
    if (activeTab_ == FrontendTab::MyCollection && collectionPage_.HandleKeyDown(event))
    {
        redraw_ = true;
        return;
    }

    if (event.key == SDLK_ESCAPE && openDropdown_ != -1)
    {
        openDropdown_ = -1;
        redraw_ = true;
        return;
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
        else if (activeTab_ == FrontendTab::About) EditCurrentProjectPage();
        break;
    case SDLK_F: ToggleFavorite(); break;
    case SDLK_1: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Rom); break;
    case SDLK_2: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Bios); break;
    case SDLK_3: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Manual); break;
    case SDLK_4: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Cover); break;
    case SDLK_5: if (activeTab_ == FrontendTab::Extras) RunImport(ImportAssetType::Screenshot); break;
    case SDLK_PAGEUP:
        if (activeTab_ == FrontendTab::Library &&
            libraryMediaMode_ == LibraryMediaMode::Screenshots)
        {
            FrontendScreenshot_Move(GetSelectedGame(), -1);
            redraw_ = true;
        }
        else if (activeTab_ == FrontendTab::About)
            SelectProjectPage(projectPageIndex_ - 1);
        else
            CycleCollectionView(-1);
        break;
    case SDLK_PAGEDOWN:
        if (activeTab_ == FrontendTab::Library &&
            libraryMediaMode_ == LibraryMediaMode::Screenshots)
        {
            FrontendScreenshot_Move(GetSelectedGame(), 1);
            redraw_ = true;
        }
        else if (activeTab_ == FrontendTab::About)
            SelectProjectPage(projectPageIndex_ + 1);
        else
            CycleCollectionView(1);
        break;
    default: break;
    }
}

void FrontendApp::HandleTextInput(const SDL_TextInputEvent& event)
{
    if (activeTab_ == FrontendTab::MyCollection)
    {
        if (collectionPage_.HandleTextInput(event))
            redraw_ = true;
        return;
    }
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
    // My Collection: D-pad navigates rows, A edits, B closes/back.
    if (activeTab_ == FrontendTab::MyCollection)
    {
        switch (event.button)
        {
        case SDL_GAMEPAD_BUTTON_DPAD_UP:
            collectionPage_.MoveSelection(-1);
            redraw_ = true;
            return;
        case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
            collectionPage_.MoveSelection(1);
            redraw_ = true;
            return;
        case SDL_GAMEPAD_BUTTON_SOUTH:
            collectionPage_.ActivateSelected();
            redraw_ = true;
            return;
        case SDL_GAMEPAD_BUTTON_EAST:
            if (!collectionPage_.CancelOrBack())
                GoBack();
            redraw_ = true;
            return;
        default:
            break;
        }
    }

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
        if (openDropdown_ != -1)
        {
            openDropdown_ = -1;
            redraw_ = true;
        }
        else
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
    // Custom Win95 chrome first: title bar (drag / buttons) and menu bar.
    if (TryChromeControlAt(event.x, event.y, event.clicks)) return;
    if (TryExitButtonAt(event.x, event.y)) return;
    if (TrySelectTabAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Manual && TryActivateManualAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::About && TryProjectControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::MyCollection &&
        collectionPage_.HandleMouseDown(event.x, event.y, event.clicks))
    {
        redraw_ = true;
        return;
    }
    if (activeTab_ == FrontendTab::Extras && TryImportControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Cartridge && TryMetadataControlAt(event.x, event.y)) return;
    if (activeTab_ == FrontendTab::Library && TryLibraryFavoriteAt(event.x, event.y, event.clicks >= 2)) return;
    if (activeTab_ == FrontendTab::Library && TryLibraryMediaControlAt(event.x, event.y)) return;
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

    if (activeTab_ == FrontendTab::About)
    {
        projectPageScroll_ += event.y > 0.0f ? -3 : 3;
        projectPageScroll_ = (std::max)(0, projectPageScroll_);
        redraw_ = true;
        return;
    }

    if (activeTab_ == FrontendTab::MyCollection)
    {
        float mouseX = 0.0f;
        float mouseY = 0.0f;
        SDL_GetMouseState(&mouseX, &mouseY);
        if (collectionPage_.HandleMouseWheel(mouseX, mouseY, event.y))
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

        const auto layout = Dashboard(panels.rightContent);

        // Library front-page My Collection summary panel: wheel scrolls its
        // compact list (Phase B).
        if (collectionPage_.HandleLibrarySummaryWheel(layout.collection,
                mouseX, mouseY, event.y))
        {
            redraw_ = true;
            return;
        }

        // Favorites panel: scroll the favorites list, only while it overflows.
        const SDL_FRect favInner{
            layout.favorites.x + 5.0f,
            layout.favorites.y + 28.0f,
            layout.favorites.w - 10.0f,
            layout.favorites.h - 33.0f
        };
        if (mouseX >= favInner.x && mouseX < favInner.x + favInner.w &&
            mouseY >= favInner.y && mouseY < favInner.y + favInner.h)
        {
            std::size_t favoriteCount = 0;
            for (const GameInfo& candidate : library_.Games())
                if (candidate.favorite) ++favoriteCount;
            if (FavoritesScrollMetrics(favInner, favoriteCount).maxScroll > 0)
            {
                favoritesScroll_ += event.y > 0.0f ? -1 : 1;
                favoritesScroll_ = (std::clamp)(favoritesScroll_, 0,
                    FavoritesScrollMetrics(favInner, favoriteCount).maxScroll);
                redraw_ = true;
                return;
            }
        }

        const auto description = layout.description;
        if (mouseX >= description.x && mouseX < description.x+description.w &&
            mouseY >= description.y && mouseY < description.y+description.h)
        {
            libraryDescriptionScroll_ += event.y > 0.0f ? -3 : 3;
            libraryDescriptionScroll_ = (std::max)(0, libraryDescriptionScroll_);
            redraw_ = true;
            return;
        }

        // Game Library list: wheel scrolls the viewport one row per notch,
        // only while the current collection/view overflows. Selection is
        // untouched; the selected game may scroll out of view (Win95 list
        // behavior). Otherwise fall through to the old selection stepping.
        const SDL_FRect listRect = LibraryListRect(panels.leftContent);
        if (mouseX >= listRect.x && mouseX < listRect.x + listRect.w &&
            mouseY >= listRect.y && mouseY < listRect.y + listRect.h)
        {
            const int itemCount = static_cast<int>(collections_.Count());
            if (LibraryListScrollMetrics(listRect, itemCount).maxScroll > 0)
            {
                libraryListScroll_ += event.y > 0.0f ? -1 : 1;
                libraryListScroll_ = (std::clamp)(libraryListScroll_, 0,
                    LibraryListScrollMetrics(listRect, itemCount).maxScroll);
                redraw_ = true;
                return;
            }
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

void FrontendApp::ToggleWindowMaximize()
{
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    if (flags & SDL_WINDOW_MAXIMIZED)
        SDL_RestoreWindow(window_);
    else
        SDL_MaximizeWindow(window_);
    redraw_ = true;
}

bool FrontendApp::BeginWindowResizeAt(float x, float y, int windowWidth,
    int windowHeight)
{
    if (!window_)
        return false;

    // Maximized / fullscreen windows are not edge-resized.
    const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
    if (flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN))
        return false;

    constexpr float edge = 6.0f;
    int edges = 0;
    if (x <= edge)
        edges |= 1; // left
    if (x >= static_cast<float>(windowWidth) - edge)
        edges |= 2; // right
    if (y <= edge)
        edges |= 4; // top
    if (y >= static_cast<float>(windowHeight) - edge)
        edges |= 8; // bottom
    if (edges == 0)
        return false;

    float globalX = 0.0f;
    float globalY = 0.0f;
    SDL_GetGlobalMouseState(&globalX, &globalY);
    resizeStartMouseX_ = globalX;
    resizeStartMouseY_ = globalY;
    SDL_GetWindowPosition(window_, &resizeStartX_, &resizeStartY_);
    SDL_GetWindowSize(window_, &resizeStartW_, &resizeStartH_);
    windowResizeEdges_ = edges;
    return true;
}

bool FrontendApp::TryChromeControlAt(float x, float y, int clicks)
{
    if (!window_)
        return false;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    (void)windowHeight;

    switch (FrontendChrome::HitTestWindowButton(windowWidth, x, y))
    {
    case FrontendChrome::WindowButton::Minimize:
        SDL_MinimizeWindow(window_);
        return true;
    case FrontendChrome::WindowButton::Maximize:
        ToggleWindowMaximize();
        return true;
    case FrontendChrome::WindowButton::Close:
        running_ = false;
        return true;
    default:
        break;
    }

    // Borderless window edge resize (before the title bar drag so the top
    // edge resizes while the title bar still moves the window).
    if (BeginWindowResizeAt(x, y, windowWidth, windowHeight))
        return true;

    if (FrontendChrome::IsInTitleBar(x, y))
    {
        // Title bar drag moves the borderless window. Maximized / fullscreen
        // windows are not dragged (classic Windows behaviour).
        const SDL_WindowFlags flags = SDL_GetWindowFlags(window_);
        if (clicks >= 2)
        {
            ToggleWindowMaximize();
            return true;
        }
        if (!(flags & (SDL_WINDOW_MAXIMIZED | SDL_WINDOW_FULLSCREEN)))
        {
            float globalX = 0.0f;
            float globalY = 0.0f;
            SDL_GetGlobalMouseState(&globalX, &globalY);
            int windowX = 0;
            int windowY = 0;
            SDL_GetWindowPosition(window_, &windowX, &windowY);
            windowDragOffsetX_ = globalX - static_cast<float>(windowX);
            windowDragOffsetY_ = globalY - static_cast<float>(windowY);
            windowDragActive_ = true;
        }
        return true;
    }

    // Menu bar is a visual shell for now; consume clicks so they never fall
    // through to the content underneath.
    if (FrontendChrome::MenuItemAt(windowWidth, x, y) >= 0)
        return true;

    return false;
}

bool FrontendApp::TrySelectTabAt(float x, float y)
{
    const float barY = FrontendChrome::TabsTop;
    const float barH = FrontendChrome::TabsHeight;
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
    const SDL_FRect listRect = LibraryListRect(content);
    const int itemCount = static_cast<int>(collections_.Count());

    if (x < content.x || x >= content.x + content.w ||
        y < listRect.y || y >= content.y + content.h)
    {
        return false;
    }

    // Scrollbar column: let the scrollbar handlers own clicks there.
    // While a thumb drag is in progress the mouse may hover anywhere -
    // row picking must not swallow the motion-driven updates.
    if (!libraryListScrollDrag_ &&
        LibraryListScrollPartAt(x, y, listRect, itemCount) != 0)
    {
        return HandleLibraryListScrollbarAt(x, y, listRect, itemCount);
    }

    // Row picking uses the live viewport; empty filler rows (index >=
    // itemCount) do nothing.
    const int row = static_cast<int>((y - listRect.y) / 28.0f);
    if (row < 0 || row >= static_cast<int>(listRect.h / 28.0f))
        return false;

    const int position = libraryListScroll_ + row;
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
    if (activeTab_ != FrontendTab::Settings) return false;
    int ww=0, wh=0; SDL_GetWindowSize(window_,&ww,&wh); const auto panels=FrontendPanels_Calculate(ww,wh);
    const float innerX=panels.rightContent.x+18.0f, innerY=panels.rightContent.y+18.0f;
    const float controlX=innerX+34.0f, comboW=(std::min)(430.0f,panels.rightContent.w-126.0f);
    auto inside=[x,y](const SDL_FRect&r){return x>=r.x&&x<r.x+r.w&&y>=r.y&&y<r.y+r.h;};
    if(openDropdown_==1){ SDL_FRect p{controlX,innerY+179.0f,comboW,90.0f}; if(inside(p)){int i=(int)((y-p.y)/30.0f); settings_.region_mode=i==0?RegionMode::Auto:(i==1?RegionMode::PAL:RegionMode::NTSC); SaveSettings(settingsPath_,settings_); openDropdown_=-1; redraw_=true; return true;} openDropdown_=-1; redraw_=true; return true; }
    else if(openDropdown_==3){ RefreshInstalledBiosFiles(); SDL_FRect p{controlX,innerY+308.0f,comboW,30.0f*(float)installedBiosFiles_.size()}; if(inside(p)&&!installedBiosFiles_.empty()){int i=(int)((y-p.y)/30.0f); if(i>=0&&i<(int)installedBiosFiles_.size()) settings_.bios_file=installedBiosFiles_[i]; SaveSettings(settingsPath_,settings_); openDropdown_=-1; redraw_=true; return true;} openDropdown_=-1; redraw_=true; return true; }
    SDL_FRect full{controlX,innerY+72.0f,420.0f,28.0f}; SDL_FRect region{controlX,innerY+126.0f,comboW,58.0f}; SDL_FRect scan{controlX,innerY+201.0f,250.0f,30.0f}; SDL_FRect bios{controlX,innerY+255.0f,comboW,58.0f};
    if(inside(full)){settingsSelected_=0; ActivateSettingsSelection(); return true;}
    if(inside(region)){settingsSelected_=1; openDropdown_=1; redraw_=true; return true;}
    if(inside(scan)){settingsSelected_=2; ActivateSettingsSelection(); return true;}
    if(inside(bios)){settingsSelected_=3; openDropdown_=3; redraw_=true; return true;}
    return TrySettingsUpdateControlAt(x, y);
}

// Phase 2 update-check controls (manual only). Mouse-down arms the CHECK
// FOR UPDATES pressed state; HandleMouseButtonUp performs the action on
// release inside the same rect, matching the Library OPEN buttons. VIEW
// RELEASE opens the public GitHub release page in the user's browser via
// the existing ShellExecuteW pattern - it never downloads anything.
bool FrontendApp::TrySettingsUpdateControlAt(float x, float y)
{
    if (activeTab_ != FrontendTab::Settings)
        return false;
    int ww = 0, wh = 0;
    SDL_GetWindowSize(window_, &ww, &wh);
    const auto panels = FrontendPanels_Calculate(ww, wh);
    const float innerX = panels.rightContent.x + 18.0f;
    const float innerY = panels.rightContent.y + 18.0f;
    const float controlX = innerX + 34.0f;
    const auto inside = [x, y](const SDL_FRect& r)
    {
        return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
    };
    const SDL_FRect updatesButton{controlX, innerY + 412.0f, 170.0f, 32.0f};
    if (inside(updatesButton))
    {
        updateCheckPressed_ = true;
        redraw_ = true;
        return true;
    }
    if (updateCheck_.GetState() == UpdateCheck::State::UpdateAvailable)
    {
        const SDL_FRect viewRelease{controlX + 190.0f, innerY + 412.0f, 150.0f, 32.0f};
        if (inside(viewRelease))
        {
            viewReleasePressed_ = true;
            redraw_ = true;
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

    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderClear(renderer_);

    // Classic Win95 window chrome: title bar + menu bar are drawn inside the
    // borderless client area, then the framed banner and navigation row.
    FrontendChrome::Draw(window_, renderer_,
        "O2EM-NG - Philips Videopac G7000 - Classic Gaming Collection - \xC2\xA9 2026 Bengt-Ove Peltz");
    FrontendLayout_DrawHeader(window_, renderer_);
    FrontendTabs_Draw(renderer_, windowWidth, activeTab_);

    const FrontendPanelLayout panels =
        FrontendPanels_Calculate(windowWidth, windowHeight);

    FrontendPanels_Draw(renderer_, panels);
    DrawLibraryList(panels.leftContent);
    DrawActiveTab(panels.rightContent);

    std::string status = "Tab: ";
    status += FrontendTabs_GetName(activeTab_);
    if (activeTab_ == FrontendTab::MyCollection)
    {
        status += collectionPage_.StatusText(library_);
    }
    else if (activeTab_ == FrontendTab::Extras)
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
    const float left = content.x + 10.0f;
    const float width = content.w - 20.0f;
    const float titleX = left + 57.0f;
    // ROM-presence colors for the catalogue number: restrained Win95-era
    // tones that stay readable on both the alternating light rows and the
    // navy selected row.
    constexpr SDL_Color kCatalogGreen{0, 128, 0, 255};   // ROM installed
    constexpr SDL_Color kCatalogRed{160, 0, 0, 255};     // catalogue entry, ROM missing
    // Visible list viewport (below the header) and its scrollbar state.
    const SDL_FRect listRect = LibraryListRect(content);
    const FavoritesScrollGeom listScroll =
        LibraryListScrollMetrics(listRect, static_cast<int>(collections_.Count()));
    const bool hasScrollbar = listScroll.maxScroll > 0;
    // Reserve the scrollbar column from the row area so no content is
    // drawn underneath it.
    const float listW = hasScrollbar ? listRect.w - 17.0f : listRect.w;
    const float favoriteX = left + listW - 28.0f;
    // Vector star stays sharp and does not depend on font glyph availability.
    const auto star = [&](float cx, float cy)
    {
        const SDL_FPoint points[] = {{0,-8},{2,-3},{8,-3},{4,1},{5,7},
            {0,4},{-5,7},{-4,1},{-8,-3},{-2,-3}};
        SDL_Vertex vertices[11]{};
        const SDL_FColor gold{1.0f,0.76f,0.08f,1.0f};
        vertices[0].position = {cx,cy}; vertices[0].color = gold;
        int indices[30];
        for (int i=0;i<10;++i)
        {
            vertices[i+1].position = {cx+points[i].x,cy+points[i].y};
            vertices[i+1].color = gold;
            indices[i*3]=0; indices[i*3+1]=i+1; indices[i*3+2]=(i+1)%10+1;
        }
        SDL_RenderGeometry(renderer_,nullptr,vertices,11,indices,30);
        SDL_SetRenderDrawColor(renderer_,128,82,0,255);
        for(int i=0;i<10;++i) SDL_RenderLine(renderer_,cx+points[i].x,cy+points[i].y,
            cx+points[(i+1)%10].x,cy+points[(i+1)%10].y);
    };
    Win95Theme::SetRenderColor(renderer_,Win95Theme::ActiveTitle);
    DrawText(renderer_,left+3,content.y+12,1.05f,
        activeTab_ == FrontendTab::Extras ? "Game Catalog" : "Game Library");
    Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
    DrawText(renderer_,left+3,content.y+35,0.80f,
        collections_.ViewName()+std::string("  |  Games: ")+std::to_string(collections_.Count()));
    const SDL_FRect header{left,content.y+57,width,25};
    DrawSunkenFrame(renderer_,header);
    Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
    DrawText(renderer_,left+8,header.y+3,0.95f,"#");
    DrawText(renderer_,titleX+5,header.y+3,0.95f,"Title");
    star(favoriteX+14,header.y+12);
    const int selected = static_cast<int>(collections_.CurrentPosition());
    const int count = static_cast<int>(collections_.Count());
    // The list fills the panel height: real rows from libraryListScroll_,
    // then visual-only empty rows continuing the alternating backgrounds so
    // the control reads as a full-height Win95 list. Empty rows represent no
    // game and are never selectable (row picking rejects index >= count).
    const int first = libraryListScroll_;
    const int capacity = listScroll.visibleRows;
    for(int row=0;row<capacity;++row)
    {
        const int index=first+row;
        const float rowTop=listRect.y+static_cast<float>(row)*28.0f;
        // Last visible row extends to the bottom inset so no white sliver
        // remains below the fill.
        const float rowH=(row==capacity-1)
            ? listRect.y+listRect.h-rowTop
            : 28.0f;
        const SDL_FRect rowRect{left,rowTop,listW,rowH};
        const bool realRow=index<count;
        const bool selectedRow=realRow&&index==selected;
        if(selectedRow) Win95Theme::SetRenderColor(renderer_,Win95Theme::SelectedItem);
        else SDL_SetRenderDrawColor(renderer_,index%2 ? 245:255,index%2 ? 245:255,index%2 ? 245:255,255);
        SDL_RenderFillRect(renderer_,&rowRect);
        Win95Theme::SetRenderColor(renderer_,Win95Theme::Light);
        SDL_RenderLine(renderer_,left,rowTop+rowH-1.0f,left+listW,rowTop+rowH-1.0f);
        SDL_RenderLine(renderer_,titleX,rowTop,titleX,rowTop+rowH);
        SDL_RenderLine(renderer_,favoriteX,rowTop,favoriteX,rowTop+rowH);
        if(!realRow)
            continue;
        const GameInfo* game=collections_.Get(static_cast<std::size_t>(index));
        // The catalogue number doubles as the ROM-presence indicator:
        // green = installed/playable, red = known entry without its ROM.
        // Titles never change color. No-number entries leave the # cell
        // empty. On the navy selected row the tint stays light for contrast.
        const std::string numberText=DisplayCatalogId(game);
        if(!numberText.empty())
        {
            const bool installed=game && (!game->romPath.empty() || !game->rom.path.empty());
            if(selectedRow)
                SDL_SetRenderDrawColor(renderer_,144,238,144,255);
            else
                Win95Theme::SetRenderColor(renderer_,installed ? kCatalogGreen : kCatalogRed);
            // Patch 0031: fit the number into the # column. Long user IDs
            // (e.g. "Tutankham+") overflowed into the Title column and
            // produced a doubled-text artifact; overflow now truncates.
            std::string numberCell=numberText;
            const auto maxNumberChars=static_cast<std::size_t>(
                (std::max)(6.0f, (titleX - (left + 5.0f) - 6.0f) / (8.0f * 0.95f)));
            if(numberCell.size()>maxNumberChars)
                numberCell=numberCell.substr(0, maxNumberChars-3)+"...";
            DrawText(renderer_,left+5,rowTop+4,0.95f,numberCell);
        }
        Win95Theme::SetRenderColor(renderer_,selectedRow ? Win95Theme::SelectedItemText : Win95Theme::WindowText);
        std::string title=game ? game->title : "";
        if(activeTab_==FrontendTab::Extras)
            title=(game && (!game->romPath.empty() || !game->rom.path.empty()) ? "[x] " : "[ ] ")+title;
        if(game && collections_.View()==CollectionView::MostPlayed)
            title+=" ["+std::to_string(game->playCount)+"]";
        const auto maxChars=static_cast<std::size_t>((std::max)(32.0f,favoriteX-titleX-12)/(8.0f*0.95f));
        if(title.size()>maxChars) title=title.substr(0,maxChars-3)+"...";
        DrawText(renderer_,titleX+5,rowTop+4,0.95f,title);
        if(game && game->favorite) star(favoriteX+14,rowTop+14);
    }
    if(!count)
    {
        Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
        DrawText(renderer_,left+5,content.y+90,0.9f,"No games in this view");
    }

    // Win95 vertical scrollbar on the right edge, only when the current
    // collection/view overflows the visible capacity.
    if(hasScrollbar)
    {
        DrawWin95VScrollbar(renderer_, listScroll.upArrow, listScroll.downArrow,
            listScroll.track, listScroll.thumb);
    }
}

void FrontendApp::DrawLibraryDashboard(const SDL_FRect& content)
{
    const GameInfo* game = GetSelectedGame();
    const auto layout=Dashboard(content);
    const auto& coverFrame=layout.cover;
    const auto& infoFrame=layout.info;
    const auto& quickFrame=layout.quick;
    const auto& favoritesFrame=layout.favorites;
    const auto& descriptionFrame=layout.description;

    // Classic group boxes: recessed outline with the caption interrupting the top edge.
    const auto drawGroup = [&](const SDL_FRect& rect, const char* caption)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &rect);
        SDL_FRect outline{rect.x, rect.y + 9.0f, rect.w - 1.0f, rect.h - 10.0f};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_FRect light{outline.x + 1.0f, outline.y + 1.0f, outline.w, outline.h};
        SDL_RenderRect(renderer_, &light);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderRect(renderer_, &outline);
        const float captionWidth = static_cast<float>(std::char_traits<char>::length(caption)) * 8.0f + 12.0f;
        const SDL_FRect backing{rect.x + 9.0f, rect.y, (std::min)(captionWidth, rect.w - 18.0f), 21.0f};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &backing);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
        DrawText(renderer_, rect.x + 14.0f, rect.y + 1.0f, 1.0f, caption);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &content);
    drawGroup(coverFrame, "Cover / Media");
    drawGroup(infoFrame, "Game Information");
    drawGroup(quickFrame, "Emulator Settings");
    drawGroup(favoritesFrame, "Favorites");
    drawGroup(descriptionFrame, "Description");
    drawGroup(layout.system, "System Information");
    drawGroup(layout.imports, "Library Quick Add");
    drawGroup(layout.recent, "Recently Played");
    drawGroup(layout.collection, "My Collection");
    if(layout.hasRightColumn)
    {
        drawGroup(layout.folders,
            layout.compactFolders ? "Folders" : "Library Folders");
        drawGroup(layout.stats,
            layout.compactFolders ? "Statistics" : "Collection Statistics");
    }

    // Cover / Media: the panel is the main media viewer. Box Art shows the
    // game cover; Screenshots embeds the (relocated) screenshot/video viewer
    // with live MP4/GIF playback. Box Art / Screenshots buttons select the
    // mode; Scale Image stays associated with the viewer.
    const SDL_FRect coverInner = LibraryCoverMediaRect(coverFrame);
    SDL_FRect mediaBoxArt{}, mediaScreenshots{};
    LibraryMediaButtonRects(coverFrame, mediaBoxArt, mediaScreenshots);
    if (libraryMediaMode_ == LibraryMediaMode::BoxArt)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
        SDL_RenderFillRect(renderer_, &coverInner);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        FrontendBoxArt_DrawImage(renderer_, coverInner, game, scaleCoverImage_);
    }
    else
    {
        FrontendScreenshot_Draw(renderer_, coverInner, game, true);
    }
    const bool boxArtActive = (libraryMediaMode_ == LibraryMediaMode::BoxArt);
    // Win95 tab-style toggle: active side uses the pressed edge (dark
    // top/left, bright bottom/right), inactive side the raised edge - the
    // same edge idiom as the top tab strip (frontend_tabs.cpp).
    const auto drawMediaButton = [&](const SDL_FRect& rect, const char* label,
        bool active)
    {
        Win95Theme::SetRenderColor(renderer_,
            active ? Win95Theme::TabActive : Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &rect);
        if (active)
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::DarkShadow);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
            SDL_RenderLine(renderer_, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer_, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
            SDL_RenderLine(renderer_, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer_, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        }
        Win95Theme::SetRenderColor(renderer_,
            active ? Win95Theme::TabActiveText : Win95Theme::WindowText);
        DrawText(renderer_, rect.x + 14.0f + (active ? 1.0f : 0.0f),
            rect.y + 6.0f + (active ? 1.0f : 0.0f), 0.85f, label);
    };
    drawMediaButton(mediaBoxArt, "Box Art", boxArtActive);
    drawMediaButton(mediaScreenshots, "Screenshots", !boxArtActive);
    if (boxArtActive && game && !game->boxArt.empty())
    {
        // Box Art mode's per-item DELETE: deletes ONLY the displayed cover.
        const SDL_FRect boxArtDelete = LibraryBoxArtDeleteRect(coverFrame);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &boxArtDelete);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, boxArtDelete.x, boxArtDelete.y,
            boxArtDelete.x + boxArtDelete.w - 1.0f, boxArtDelete.y);
        SDL_RenderLine(renderer_, boxArtDelete.x, boxArtDelete.y,
            boxArtDelete.x, boxArtDelete.y + boxArtDelete.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderLine(renderer_, boxArtDelete.x, boxArtDelete.y + boxArtDelete.h - 1.0f,
            boxArtDelete.x + boxArtDelete.w - 1.0f, boxArtDelete.y + boxArtDelete.h - 1.0f);
        SDL_RenderLine(renderer_, boxArtDelete.x + boxArtDelete.w - 1.0f, boxArtDelete.y,
            boxArtDelete.x + boxArtDelete.w - 1.0f, boxArtDelete.y + boxArtDelete.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, boxArtDelete.x + 22.0f, boxArtDelete.y + 7.0f, 0.85f, "DELETE");
    }
    const SDL_FRect scaleBox{coverFrame.x+10,coverFrame.y+coverFrame.h-26,17,17};
    DrawSunkenFrame(renderer_,scaleBox);
    Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
    if(scaleCoverImage_) DrawText(renderer_,scaleBox.x+2,scaleBox.y-2,0.85f,"x");
    DrawText(renderer_,scaleBox.x+25,scaleBox.y-1,0.85f,"Scale image");

    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    const SDL_FRect infoInner{
        infoFrame.x + 5.0f,
        infoFrame.y + 28.0f,
        infoFrame.w - 10.0f,
        infoFrame.h - 33.0f
    };
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &infoInner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);

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

    const auto smallLine=[&](const SDL_FRect& r,float offset,const std::string& text)
    {
        Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
        DrawText(renderer_,r.x+10,r.y+offset,0.84f,fitText(text,r.w-20,0.84f));
    };
    smallLine(layout.system,24,"BIOS: "+(settings_.bios_file.empty() ? std::string("Not selected") : settings_.bios_file));
    smallLine(layout.system,48,"Region setting: "+RegionModeToString(settings_.region_mode));
    smallLine(layout.system,72,"Frontend: SDL3 / Windows");
#ifdef _WIN64
    smallLine(layout.system,96,"Architecture: x64");
#else
    smallLine(layout.system,96,"Architecture: x86");
#endif
#ifdef _DEBUG
    smallLine(layout.system,120,"Build: Debug");
#else
    smallLine(layout.system,120,"Build: Release");
#endif
    // C7010/C7420 NSC800 expansion firmware status, from the same detection
    // the Settings screen reports (RefreshInstalledBiosFiles). One authoritative
    // source; no duplicated state. The compact wording keeps the narrow panel clean.
    smallLine(layout.system,144,"C7010 NSC800: "+std::string(c7010FirmwareInstalled_ ? "Installed" : "Not found"));
    smallLine(layout.system,168,"C7420 NSC800: "+std::string(c7420FirmwareInstalled_ ? "Installed" : "Not found"));
    const char* importLabels[]={"Import ROM...","Import Cover...","Import Manual..."};
    for(int i=0;i<3;++i)
    {
        // Win95 push button with 3px offset drop shadow, matching the
        // reference design; renders sunken with the content nudged 2px
        // down/right while the left button is held on it. The rect comes
        // from QuickAddButtonRect() so it always matches the hitboxes in
        // TryLibraryQuickControlAt()/HandleMouseButtonUp().
        const SDL_FRect button=QuickAddButtonRect(layout.imports,i);
        const bool pressed=(quickAddPressed_==i);
        if(!pressed)
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::Shadow);
            const SDL_FRect buttonShadow{button.x+3.0f,button.y+3.0f,button.w,button.h};
            SDL_RenderFillRect(renderer_,&buttonShadow);
        }
        Win95Theme::SetRenderColor(renderer_,Win95Theme::Face);
        SDL_RenderFillRect(renderer_,&button);
        if(pressed)
        {
            // Same pressed-edge idiom as the frontend tabs: dark top/left,
            // bright bottom/right. The 1px edge sits inside the rect, so the
            // face shrinks by 2px and the content reads as pushed in.
            Win95Theme::SetRenderColor(renderer_,Win95Theme::DarkShadow);
            SDL_RenderLine(renderer_,button.x,button.y,button.x+button.w-1.0f,button.y);
            SDL_RenderLine(renderer_,button.x,button.y,button.x,button.y+button.h-1.0f);
            Win95Theme::SetRenderColor(renderer_,Win95Theme::Highlight);
            SDL_RenderLine(renderer_,button.x,button.y+button.h-1.0f,button.x+button.w-1.0f,button.y+button.h-1.0f);
            SDL_RenderLine(renderer_,button.x+button.w-1.0f,button.y,button.x+button.w-1.0f,button.y+button.h-1.0f);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::Highlight);
            SDL_RenderLine(renderer_,button.x,button.y,button.x+button.w-1.0f,button.y);
            SDL_RenderLine(renderer_,button.x,button.y,button.x,button.y+button.h-1.0f);
            Win95Theme::SetRenderColor(renderer_,Win95Theme::Shadow);
            SDL_RenderLine(renderer_,button.x,button.y+button.h-1.0f,button.x+button.w-1.0f,button.y+button.h-1.0f);
            SDL_RenderLine(renderer_,button.x+button.w-1.0f,button.y,button.x+button.w-1.0f,button.y+button.h-1.0f);
        }
        // 16px procedural pixel glyph on the left, dark outline + mid body:
        // cartridge / picture / book, echoing the reference mock.
        const float pressOffset=pressed?2.0f:0.0f;
        const float glyphX=button.x+8.0f+pressOffset, glyphY=button.y+(button.h-16.0f)*0.5f+pressOffset;
        Win95Theme::SetRenderColor(renderer_,Win95Theme::DarkShadow);
        const SDL_FRect glyphFrame{glyphX,glyphY,16.0f,16.0f};
        SDL_RenderRect(renderer_,&glyphFrame);
        Win95Theme::SetRenderColor(renderer_,Win95Theme::Shadow);
        const SDL_FRect glyphBody{glyphX+2.0f,glyphY+2.0f,12.0f,12.0f};
        SDL_RenderFillRect(renderer_,&glyphBody);
        Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
        const float glyphTextY=glyphY+4.0f;
        const auto glyphRect=[&](float rx,float ry,float rw,float rh)
        {
            const SDL_FRect r{glyphX+rx,glyphTextY+ry,rw,rh};
            SDL_RenderFillRect(renderer_,&r);
        };
        switch(i)
        {
        case 0: // cartridge: two label bars
            glyphRect(4.0f,3.0f,8.0f,2.0f);
            glyphRect(4.0f,8.0f,8.0f,2.0f);
            break;
        case 1: // picture: mountains + sun
            glyphRect(3.0f,7.0f,4.0f,4.0f);
            glyphRect(6.0f,5.0f,4.0f,6.0f);
            glyphRect(9.0f,3.0f,3.0f,3.0f);
            break;
        case 2: // open book: two page wedges
            glyphRect(3.0f,2.0f,4.0f,2.0f);
            glyphRect(3.0f,5.0f,4.0f,2.0f);
            glyphRect(3.0f,8.0f,4.0f,2.0f);
            glyphRect(9.0f,2.0f,4.0f,2.0f);
            glyphRect(9.0f,5.0f,4.0f,2.0f);
            glyphRect(9.0f,8.0f,4.0f,2.0f);
            break;
        }
        DrawText(renderer_,button.x+30.0f+pressOffset,button.y+4+pressOffset,0.84f,importLabels[i]);
    }
    std::vector<const GameInfo*> recent;
    for(const auto& item:library_.Games()) if(item.lastPlayed>0) recent.push_back(&item);
    std::stable_sort(recent.begin(),recent.end(),[](const GameInfo* a,const GameInfo* b){return a->lastPlayed>b->lastPlayed;});
    for(std::size_t i=0;i<recent.size() && i<4;++i)
        smallLine(layout.recent,30+24.0f*static_cast<float>(i),DisplayCatalogId(recent[i])+" "+recent[i]->title);
    if(recent.empty()) smallLine(layout.recent,30,"No games played yet");
    // Library front-page My Collection summary (Phase B): compact list of
    // collection entries plus owned/boxed/manuals counts. The group frame and
    // caption are drawn by drawGroup above.
    collectionPage_.DrawLibrarySummary(renderer_, layout.collection);

    // Far-right Library Folders / Collection Statistics groups (drawn only
    // when the responsive layout provides the optional column).
    if(layout.hasRightColumn)
    {
        static const char* folderLabels[]={"ROMs","Box Art","Screenshots / Media",
            "Manuals","BIOS / Firmware"};
        // Minimum-content fallback: short row labels in compact mode so they
        // can never collide with the OPEN buttons at narrow widths. Full
        // labels are drawn verbatim at comfortable widths (unchanged).
        static const char* compactFolderLabels[]={"ROMs","Cover","Media",
            "Docs","Firmware"};
        const char* const* rowLabels=layout.compactFolders
            ? compactFolderLabels : folderLabels;
        for(int i=0;i<5;++i)
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
            DrawText(renderer_,layout.folders.x+10,layout.folders.y+29+i*28,0.84f,
                layout.compactFolders
                    ? fitText(rowLabels[i],layout.folders.w-86.0f,0.84f)
                    : std::string(rowLabels[i]));
            const SDL_FRect openButton=LibraryOpenButtonRect(layout.folders,i);
            DrawWin95Button(openButton,"OPEN",folderOpenPressed_==i);
        }

        // Subtle footer note in the spare space under the last OPEN row
        // (pure text; panel geometry unchanged). Omitted in compact mode,
        // where the panel is too narrow for it to read well.
        if(!layout.compactFolders)
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
            DrawText(renderer_,layout.folders.x+10,layout.folders.y+166,0.72f,
                "Folder access:");
            DrawText(renderer_,layout.folders.x+10,layout.folders.y+180,0.72f,
                "Opens in Windows Explorer");
        }

        const auto statLine=[&](float offset,const std::string& text)
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::WindowText);
            DrawText(renderer_,layout.stats.x+10,layout.stats.y+offset,0.84f,
                fitText(text,layout.stats.w-20,0.84f));
        };
        std::size_t favoritesCount=0,boxArtCount=0,screenshotCount=0,
            videoCount=0,manualCount=0;
        for(const GameInfo& item:library_.Games())
        {
            if(item.favorite) ++favoritesCount;
            if(!item.boxArt.empty()) ++boxArtCount;
            if(!item.manual.empty()) ++manualCount;
            for(const std::filesystem::path& shot:item.screenshots)
            {
                std::string extension=shot.extension().string();
                std::transform(extension.begin(),extension.end(),extension.begin(),
                    [](unsigned char c){return static_cast<char>(std::tolower(c));});
                if(extension==".mp4") ++videoCount; else ++screenshotCount;
            }
        }
        // Two visual sections in the existing blue label color; same values,
        // same counting logic - presentation only.
        const auto statCaption=[&](float offset,const std::string& text)
        {
            Win95Theme::SetRenderColor(renderer_,Win95Theme::ActiveTitle);
            DrawText(renderer_,layout.stats.x+10,layout.stats.y+offset,0.84f,
                fitText(text,layout.stats.w-20,0.84f));
        };
        statCaption(22,"Library");
        statLine(42,"Games: "+std::to_string(library_.Count()));
        statLine(62,"Favorites: "+std::to_string(favoritesCount));
        statCaption(84,"Media");
        statLine(104,"Box Art: "+std::to_string(boxArtCount));
        statLine(124,"Screenshots: "+std::to_string(screenshotCount));
        statLine(144,"Videos: "+std::to_string(videoCount));
        statLine(164,"Manuals: "+std::to_string(manualCount));
    }

    const std::string catalogId = DisplayCatalogId(game);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
    DrawText(renderer_, infoInner.x + 10.0f, infoInner.y + 10.0f, 0.90f, "Videopac No.");
    // Compact catalogue-number plate: navy ground with a heavy colored
    // number - green tones when the ROM is installed, red tones when the
    // catalogue entry has no ROM. Left blank when the game has no catalogue
    // number (the Title row stays the authoritative name display).
    const SDL_FRect numberBox{infoInner.x + 112.0f, infoInner.y + 3.0f,
        (std::min)(66.0f, (std::max)(34.0f, infoInner.w - 122.0f)), 30.0f};
    DrawSunkenFrame(renderer_, numberBox);
    if (!catalogId.empty())
    {
        const bool numberInstalled = !game->romPath.empty() || !game->rom.path.empty();
        Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
        const SDL_FRect plate{numberBox.x + 2.0f, numberBox.y + 2.0f,
            numberBox.w - 4.0f, numberBox.h - 4.0f};
        SDL_RenderFillRect(renderer_, &plate);
        if (numberInstalled)
            SDL_SetRenderDrawColor(renderer_, 144, 238, 144, 255);
        else
            SDL_SetRenderDrawColor(renderer_, 255, 130, 130, 255);
        // Bold face + true text measurement (Patch 0031): the number is
        // horizontally AND vertically centered in the plate using the real
        // rendered extent instead of a per-character estimate, so 01, 54+,
        // C7010 and 55+ all sit centered and read as a catalogue plate.
        // UiFont draws at native point size - no extra scale factor.
        constexpr float kNumberPointSize = 18.0f;
        float textW = 0.0f;
        float textH = 0.0f;
        const bool measured = UiFont_MeasureText(kNumberPointSize, catalogId, &textW, &textH, /*bold=*/true);
        const float drawX = measured
            ? plate.x + (std::max)(3.0f, (plate.w - textW) * 0.5f)
            : plate.x + (std::max)(3.0f, (plate.w - static_cast<float>(catalogId.size()) * 8.0f * 1.35f) * 0.5f);
        const float drawY = plate.y + (std::max)(2.0f, (plate.h - (measured ? textH : 20.0f)) * 0.5f);
        if (!UiFont_DrawTextBold(renderer_, drawX, drawY, kNumberPointSize, catalogId))
            DrawText(renderer_, drawX, drawY, 1.35f, catalogId);
    }
    float y = infoInner.y + 49.0f;
    const std::pair<std::string, std::string> rows[] = {
        {"Title:", game->title},
        {"Publisher:", game->publisher},
        {"Developer:", game->developer},
        {"Year:", game->year},
        {"Genre:", game->genre},
        {"Players:", game->players},
        {"Controls:", game->controls},
        {"Voice Module:", game->voiceModule},
        {"Videopac+:", game->videopacPlus},
        {"Rating:", game->rating},
        {"Manual:", game->manual.empty() ? "No" : "Available"},
        {"Images/video:", std::to_string(game->screenshots.size())},
        {"Favorite:", game->favorite ? "Yes" : "No"}
    };
    const float valueX = infoInner.x + 112.0f;
    for (const auto& row : rows)
    {
        if (y > infoInner.y + infoInner.h - 20.0f) break;
        Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
        DrawText(renderer_, infoInner.x + 10.0f, y, 0.86f, row.first);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, valueX, y, 0.90f,
            fitText(valueOrDash(row.second), infoInner.x + infoInner.w - valueX - 10.0f, 0.90f));
        y += 21.0f;
    }

    // Quick Settings uses familiar Win95-style controls instead of clickable text rows.
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    const SDL_FRect quickInner{quickFrame.x + 5.0f, quickFrame.y + 28.0f, quickFrame.w - 10.0f, quickFrame.h - 33.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &quickInner);

    const auto drawCombo = [&](float yPos, const char* label, const std::string& value)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, quickInner.x + 10.0f, yPos, 0.82f, label);
        const SDL_FRect box=QuickComboRect(quickInner, yPos + 17.0f, 25.0f);
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

    drawCombo(quickInner.y + 4.0f, "BIOS",
        settings_.bios_file.empty() ? "No BIOS installed" : settings_.bios_file);
    std::string regionText = RegionModeToString(settings_.region_mode);
    std::transform(regionText.begin(), regionText.end(), regionText.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    drawCombo(quickInner.y + 50.0f, "REGION", regionText);

    const auto drawQuickCheck = [&](float yPos, const char* label, bool checked)
    {
        const SDL_FRect box{quickInner.x + 11.0f, yPos, 17.0f, 17.0f};
        DrawSunkenFrame(renderer_, box);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
        const SDL_FRect checkFill{box.x + 2.0f, box.y + 2.0f, box.w - 4.0f, box.h - 4.0f};
        SDL_RenderFillRect(renderer_, &checkFill);
        if (checked)
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
            DrawText(renderer_, box.x + 2.0f, box.y - 2.0f, 0.82f, "x");
        }
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, box.x + 25.0f, yPos - 1.0f, 0.86f, label);
    };
    drawQuickCheck(quickInner.y + 98.0f, "Scanlines", settings_.scanlines);
    // Same authoritative setting as the Settings screen's Fullscreen checkbox;
    // toggling here goes through ActivateSettingsSelection() -> ApplyFullscreenMode().
    drawQuickCheck(quickInner.y + 122.0f, "Fullscreen", settings_.start_fullscreen);

    // Drop-down lists are drawn last so they sit above the normal Quick Settings controls.
    if (openDropdown_ == 3)
    {
        RefreshInstalledBiosFiles();
        const float itemH = 25.0f;
        SDL_FRect popup{quickInner.x + 10.0f, quickInner.y + 46.0f, (quickInner.w - 20.0f) * 0.75f,
            itemH * static_cast<float>(installedBiosFiles_.size())};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window); SDL_RenderFillRect(renderer_, &popup);
        DrawSunkenFrame(renderer_, popup);
        for (int i = 0; i < static_cast<int>(installedBiosFiles_.size()); ++i)
        {
            const bool selected = installedBiosFiles_[i] == settings_.bios_file;
            SDL_FRect row{popup.x + 2.0f, popup.y + 2.0f + i * itemH, popup.w - 4.0f, itemH};
            Win95Theme::SetRenderColor(renderer_, selected ? Win95Theme::SelectedItem : Win95Theme::Window);
            SDL_RenderFillRect(renderer_, &row);
            Win95Theme::SetRenderColor(renderer_, selected ? Win95Theme::SelectedItemText : Win95Theme::WindowText);
            DrawText(renderer_, popup.x + 6.0f, popup.y + 3.0f + i * itemH, 0.82f, installedBiosFiles_[i]);
        }
    }
    else if (openDropdown_ == 1)
    {
        const char* items[] = {"AUTO", "PAL", "NTSC"}; const float itemH = 25.0f;
        SDL_FRect popup{quickInner.x + 10.0f, quickInner.y + 92.0f, (quickInner.w - 20.0f) * 0.75f, itemH * 3.0f};
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window); SDL_RenderFillRect(renderer_, &popup);
        DrawSunkenFrame(renderer_, popup);
        const int selectedRegion = settings_.region_mode == RegionMode::Auto ? 0 : (settings_.region_mode == RegionMode::PAL ? 1 : 2);
        for (int i = 0; i < 3; ++i)
        {
            SDL_FRect row{popup.x + 2.0f, popup.y + 2.0f + i * itemH, popup.w - 4.0f, itemH};
            Win95Theme::SetRenderColor(renderer_, i == selectedRegion ? Win95Theme::SelectedItem : Win95Theme::Window);
            SDL_RenderFillRect(renderer_, &row);
            Win95Theme::SetRenderColor(renderer_, i == selectedRegion ? Win95Theme::SelectedItemText : Win95Theme::WindowText);
            DrawText(renderer_, popup.x + 6.0f, popup.y + 3.0f + i * itemH, 0.82f, items[i]);
        }
    }

    // Favorites are read from the live library state and sorted alphabetically.
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::ActiveTitle);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    const SDL_FRect favInner{favoritesFrame.x + 5.0f, favoritesFrame.y + 28.0f, favoritesFrame.w - 10.0f, favoritesFrame.h - 33.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &favInner);

    std::vector<const GameInfo*> favorites;
    for (const GameInfo& candidate : library_.Games())
        if (candidate.favorite) favorites.push_back(&candidate);
    std::stable_sort(favorites.begin(), favorites.end(), [](const GameInfo* a, const GameInfo* b)
    {
        return a->title < b->title;
    });

    const FavoritesScrollGeom scrollGeom =
        FavoritesScrollMetrics(favInner, favorites.size());
    const int maximumRows = scrollGeom.visibleRows;
    const bool hasScrollbar = scrollGeom.maxScroll > 0;
    const float listWidth = hasScrollbar ? favInner.w - 17.0f : favInner.w;
    const int first = favoritesScroll_;
    const int shown = (std::min)(maximumRows,
        static_cast<int>(favorites.size()) - first);
    float favY = favInner.y + 13.0f;
    for (int i = 0; i < shown; ++i)
    {
        const GameInfo& favorite =
            *favorites[static_cast<std::size_t>(first + i)];
        if (game && favorite.filename == game->filename)
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::SelectedItem);
            const SDL_FRect highlight{favInner.x + 5.0f, favY - 4.0f, listWidth - 10.0f, 22.0f};
            SDL_RenderFillRect(renderer_, &highlight);
            SDL_SetRenderDrawColor(renderer_, 255, 255, 255, 255);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        }
        const std::string favoriteCatalogId = DisplayCatalogId(&favorite);
        std::string favLine = favoriteCatalogId.empty() ? "--" : favoriteCatalogId;
        favLine += "  " + favorite.title;
        DrawText(renderer_, favInner.x + 12.0f, favY, 0.84f,
            fitText(favLine, listWidth - 24.0f, 0.84f));
        favY += 24.0f;
    }
    if (favorites.empty())
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_, favInner.x + 12.0f, favY, 0.84f, "No favorites added");
    }

    // Win95 vertical scrollbar (right side), only when the list overflows.
    // Same raised-edge idiom as the rest of the dashboard controls.
    if (hasScrollbar)
    {
        const auto drawArrowButton = [&](const SDL_FRect& rect, bool up)
        {
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
            SDL_RenderFillRect(renderer_, &rect);
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer_, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
            SDL_RenderLine(renderer_, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer_, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
            const float cx = rect.x + rect.w * 0.5f;
            const float cy = up ? rect.y + rect.h * 0.5f - 1.5f
                                : rect.y + rect.h * 0.5f + 1.5f;
            for (int k = 0; k < 4; ++k)
            {
                const float half = static_cast<float>(k);
                const float yy = up ? cy - 1.5f + static_cast<float>(k)
                                    : cy + 1.5f - static_cast<float>(k);
                SDL_RenderLine(renderer_, cx - half, yy, cx + half, yy);
            }
        };
        drawArrowButton(scrollGeom.upArrow, true);
        drawArrowButton(scrollGeom.downArrow, false);

        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &scrollGeom.track);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderLine(renderer_, scrollGeom.track.x, scrollGeom.track.y,
            scrollGeom.track.x, scrollGeom.track.y + scrollGeom.track.h - 1.0f);
        SDL_RenderLine(renderer_, scrollGeom.track.x, scrollGeom.track.y,
            scrollGeom.track.x + scrollGeom.track.w - 1.0f, scrollGeom.track.y);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, scrollGeom.track.x + scrollGeom.track.w - 1.0f, scrollGeom.track.y,
            scrollGeom.track.x + scrollGeom.track.w - 1.0f, scrollGeom.track.y + scrollGeom.track.h - 1.0f);
        SDL_RenderLine(renderer_, scrollGeom.track.x, scrollGeom.track.y + scrollGeom.track.h - 1.0f,
            scrollGeom.track.x + scrollGeom.track.w - 1.0f, scrollGeom.track.y + scrollGeom.track.h - 1.0f);

        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
        SDL_RenderFillRect(renderer_, &scrollGeom.thumb);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, scrollGeom.thumb.x, scrollGeom.thumb.y,
            scrollGeom.thumb.x + scrollGeom.thumb.w - 1.0f, scrollGeom.thumb.y);
        SDL_RenderLine(renderer_, scrollGeom.thumb.x, scrollGeom.thumb.y,
            scrollGeom.thumb.x, scrollGeom.thumb.y + scrollGeom.thumb.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderLine(renderer_, scrollGeom.thumb.x, scrollGeom.thumb.y + scrollGeom.thumb.h - 1.0f,
            scrollGeom.thumb.x + scrollGeom.thumb.w - 1.0f, scrollGeom.thumb.y + scrollGeom.thumb.h - 1.0f);
        SDL_RenderLine(renderer_, scrollGeom.thumb.x + scrollGeom.thumb.w - 1.0f, scrollGeom.thumb.y,
            scrollGeom.thumb.x + scrollGeom.thumb.w - 1.0f, scrollGeom.thumb.y + scrollGeom.thumb.h - 1.0f);
    }

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);


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

    case FrontendTab::Manual:
        DrawManualTab(content);
        break;

    case FrontendTab::Settings:
        DrawSettingsTab(content);
        break;

    case FrontendTab::About:
        DrawAboutTab(content);
        break;

    case FrontendTab::MyCollection:
        collectionPage_.Draw(renderer_, content);
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

SDL_FRect FrontendApp::QuickComboRect(const SDL_FRect& inner, float y, float h) const
{
    // 75% of the original combo width, left-aligned; shared by the draw code
    // and the hit tests so the visible BIOS/REGION controls and their click
    // targets can never drift apart.
    return SDL_FRect{inner.x + 10.0f, y, (inner.w - 20.0f) * 0.75f, h};
}

SDL_FRect FrontendApp::LibraryCoverMediaRect(const SDL_FRect& coverPanel) const
{
    // The media viewer area inside the Cover / Media panel; leaves room at
    // the bottom for the Box Art / Screenshots buttons and Scale Image.
    return SDL_FRect{
        coverPanel.x + 5.0f,
        coverPanel.y + 28.0f,
        coverPanel.w - 10.0f,
        coverPanel.h - 100.0f};
}

void FrontendApp::LibraryMediaButtonRects(const SDL_FRect& coverPanel,
    SDL_FRect& boxArt, SDL_FRect& screenshots) const
{
    const SDL_FRect viewer = LibraryCoverMediaRect(coverPanel);
    const float buttonY = viewer.y + viewer.h + 6.0f;
    const float buttonWidth = 118.0f;
    boxArt = {viewer.x + 8.0f, buttonY, buttonWidth, 26.0f};
    screenshots = {viewer.x + viewer.w - buttonWidth - 8.0f, buttonY,
        buttonWidth, 26.0f};
}

SDL_FRect FrontendApp::LibraryBoxArtDeleteRect(const SDL_FRect& coverPanel) const
{
    // Box Art mode's DELETE sits centered between the Box Art and Screenshots
    // toggle buttons, same row/height; deletes only the displayed cover file.
    const SDL_FRect viewer = LibraryCoverMediaRect(coverPanel);
    const float buttonY = viewer.y + viewer.h + 6.0f;
    constexpr float buttonWidth = 90.0f;
    return {viewer.x + (viewer.w - buttonWidth) * 0.5f, buttonY,
        buttonWidth, 26.0f};
}

bool FrontendApp::TryLibraryMediaControlAt(float x, float y)
{
    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window_, &windowWidth, &windowHeight);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(windowWidth, windowHeight);
    const auto layout = Dashboard(panels.rightContent);

    SDL_FRect boxArt{}, screenshots{};
    LibraryMediaButtonRects(layout.cover, boxArt, screenshots);
    const auto contains = [](const SDL_FRect& r, float px, float py)
    {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    };

    if (contains(boxArt, x, y))
    {
        if (libraryMediaMode_ != LibraryMediaMode::BoxArt)
        {
            // Screenshots -> Box Art: stop GIF/MP4 playback cleanly.
            FrontendScreenshot_ResetView();
            libraryMediaMode_ = LibraryMediaMode::BoxArt;
        }
        redraw_ = true;
        return true;
    }
    if (contains(screenshots, x, y))
    {
        if (libraryMediaMode_ != LibraryMediaMode::Screenshots)
        {
            libraryMediaMode_ = LibraryMediaMode::Screenshots;
        }
        redraw_ = true;
        return true;
    }

    if (libraryMediaMode_ == LibraryMediaMode::Screenshots)
    {
        const GameInfo* game = GetSelectedGame();
        const SDL_FRect viewer = LibraryCoverMediaRect(layout.cover);
        // Per-item DELETE first: deletes only the currently displayed file
        // (RunDelete resolves the path and asks for confirmation).
        if (game && !game->screenshots.empty() &&
            FrontendScreenshot_DeleteHitTest(viewer, game, x, y, true))
        {
            RunDelete(ImportAssetType::Screenshot);
            return true;
        }
        if (FrontendScreenshot_HitTest(viewer, game, x, y, true))
        {
            // The tab path repainted explicitly after HitTest; keep that
            // contract here (Move changes the index but does not redraw).
            redraw_ = true;
            return true;
        }
        return false;
    }

    // Box Art mode: DELETE removes only the displayed cover file. Refresh,
    // empty-state handling and texture invalidation all live in RunDelete.
    const GameInfo* game = GetSelectedGame();
    if (game && !game->boxArt.empty())
    {
        const SDL_FRect deleteButton = LibraryBoxArtDeleteRect(layout.cover);
        if (deleteButton.x <= x && x < deleteButton.x + deleteButton.w &&
            deleteButton.y <= y && y < deleteButton.y + deleteButton.h)
        {
            RunDelete(ImportAssetType::Cover);
            return true;
        }
    }
    return false;
}

void FrontendApp::HandleMouseButtonUp(const SDL_MouseButtonEvent& event)
{
    if (event.button != SDL_BUTTON_LEFT)
        return;
    if (windowDragActive_)
    {
        windowDragActive_ = false;
        redraw_ = true;
    }
    if (windowResizeEdges_ != 0)
    {
        windowResizeEdges_ = 0;
        redraw_ = true;
    }
    // Game Data push buttons: on release inside the same rect, run the
    // button's existing action; otherwise just restore the raised state.
    if (metadataButtonPressed_ >= 0)
    {
        const int pressed = metadataButtonPressed_;
        metadataButtonPressed_ = -1;
        redraw_ = true;
        if (activeTab_ != FrontendTab::Cartridge)
            return;
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
        const SDL_FRect c = panels.rightContent;
        const SDL_FRect inner{c.x + 18.0f, c.y + 18.0f, c.w - 36.0f, c.h - 36.0f};
        const SDL_FRect button = MetadataButtonRect(inner, pressed);
        if (event.x >= button.x && event.x < button.x + button.w &&
            event.y >= button.y && event.y < button.y + button.h)
        {
            switch (pressed)
            {
            case 0: BeginMetadataEdit(); break;
            case 1: SaveMetadataEdit(); break;
            case 2: CancelMetadataEdit(); break;
            case 3: DeleteGameDataForSelectedGame(); break;
            }
        }
        return;
    }
    if (folderOpenPressed_ >= 0)
    {
        const int pressed = folderOpenPressed_;
        folderOpenPressed_ = -1;
        redraw_ = true;
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
        const DashboardLayout& layout = Dashboard(panels.rightContent);
        if (layout.hasRightColumn)
        {
            const SDL_FRect button = LibraryOpenButtonRect(layout.folders, pressed);
            if (event.x >= button.x && event.x < button.x + button.w &&
                event.y >= button.y && event.y < button.y + button.h)
            {
                const std::filesystem::path folder = LibraryFolderPath(pressed);
                std::error_code error;
                if (!folder.empty() && std::filesystem::is_directory(folder, error) && !error)
                {
                    ShellExecuteW(nullptr, L"open", folder.wstring().c_str(),
                        nullptr, nullptr, SW_SHOWNORMAL);
                }
            }
        }
        return;
    }
    // Settings update controls: run the action on release inside the same
    // rect. CHECK UPDATES starts one background check; VIEW RELEASE
    // opens the public GitHub release page (browser only, no download).
    if (updateCheckPressed_ || viewReleasePressed_)
    {
        const bool checkPressed = updateCheckPressed_;
        const bool viewPressed = viewReleasePressed_;
        updateCheckPressed_ = false;
        viewReleasePressed_ = false;
        redraw_ = true;
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
        const SDL_FRect c = panels.rightContent;
        const float innerX = c.x + 18.0f, innerY = c.y + 18.0f;
        const float controlX = innerX + 34.0f;
        const SDL_FRect updatesButton{controlX, innerY + 412.0f, 170.0f, 32.0f};
        const SDL_FRect viewRelease{controlX + 190.0f, innerY + 412.0f, 150.0f, 32.0f};
        const auto insideRect = [](const SDL_MouseButtonEvent& e, const SDL_FRect& r)
        {
            return e.x >= r.x && e.x < r.x + r.w && e.y >= r.y && e.y < r.y + r.h;
        };
        if (checkPressed && insideRect(event, updatesButton) &&
            updateCheck_.GetState() != UpdateCheck::State::Checking)
        {
            updateCheck_.StartCheck(O2emVersion::kAppVersion);
            redraw_ = true;
        }
        else if (viewPressed && insideRect(event, viewRelease) &&
            updateCheck_.GetState() == UpdateCheck::State::UpdateAvailable)
        {
            const std::string url = updateCheck_.GetReleaseUrl();
            if (!url.empty())
            {
                const std::wstring wideUrl(url.begin(), url.end());
                ShellExecuteW(nullptr, L"open", wideUrl.c_str(),
                    nullptr, nullptr, SW_SHOWNORMAL);
            }
        }
        return;
    }
    if (quickAddPressed_ >= 0)
    {
        const int pressed = quickAddPressed_;
        quickAddPressed_ = -1;
        redraw_ = true;
        int width = 0;
        int height = 0;
        SDL_GetWindowSize(window_, &width, &height);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(width, height);
        const SDL_FRect button = QuickAddButtonRect(
            Dashboard(panels.rightContent).imports, pressed);
        if (event.x >= button.x && event.x < button.x + button.w &&
            event.y >= button.y && event.y < button.y + button.h)
        {
            const ImportAssetType types[] = {
                ImportAssetType::Rom, ImportAssetType::Cover, ImportAssetType::Manual};
            RunImport(types[pressed]);
        }
        return;
    }

    // My Collection buttons/dialog: release completes the action.
    if (activeTab_ == FrontendTab::MyCollection && collectionPage_.HandleMouseUp(event.x, event.y))
    {
        redraw_ = true;
        return;
    }

    // Game Library scrollbar thumb drag: commit the final scroll position
    // even if released outside the panel (Win95 behavior).
    if (libraryListScrollDrag_)
    {
        libraryListScrollDrag_ = false;
        redraw_ = true;
    }

    // Favorites scrollbar thumb drag: while held, the thumb follows the
    // cursor vertically (Win95 behavior) regardless of where it moves.
    if (favoritesScrollDrag_)
    {
        favoritesScrollDrag_ = false;
        if (activeTab_ == FrontendTab::Library)
        {
            int w = 0;
            int h = 0;
            SDL_GetWindowSize(window_, &w, &h);
            const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
            const SDL_FRect favInner{
                Dashboard(panels.rightContent).favorites.x + 5.0f,
                Dashboard(panels.rightContent).favorites.y + 28.0f,
                Dashboard(panels.rightContent).favorites.w - 10.0f,
                Dashboard(panels.rightContent).favorites.h - 33.0f
            };
            std::size_t favoriteCount = 0;
            for (const GameInfo& candidate : library_.Games())
                if (candidate.favorite) ++favoriteCount;
            const FavoritesScrollGeom geom =
                FavoritesScrollMetrics(favInner, favoriteCount);
            const float offset = (std::clamp)(event.y - favoritesScrollDragGrab_,
                geom.track.y, geom.track.y + geom.track.h - geom.thumb.h);
            const float freeH = (std::max)(1.0f, geom.track.h - geom.thumb.h);
            favoritesScroll_ = (std::clamp)(
                static_cast<int>((offset - geom.track.y) * static_cast<float>(geom.maxScroll) / freeH + 0.5f),
                0, geom.maxScroll);
            redraw_ = true;
        }
    }
}

void FrontendApp::HandleMouseMotion(const SDL_MouseMotionEvent& event)
{
    // Borderless window drag: while the title bar is held, follow the global
    // cursor so the window can be moved like a native caption.
    if (windowDragActive_)
    {
        float globalX = 0.0f;
        float globalY = 0.0f;
        SDL_GetGlobalMouseState(&globalX, &globalY);
        SDL_SetWindowPosition(window_,
            static_cast<int>(globalX - windowDragOffsetX_),
            static_cast<int>(globalY - windowDragOffsetY_));
    }

    // Borderless edge resize.
    if (windowResizeEdges_ != 0)
    {
        float globalX = 0.0f;
        float globalY = 0.0f;
        SDL_GetGlobalMouseState(&globalX, &globalY);
        const int dx = static_cast<int>(globalX - resizeStartMouseX_);
        const int dy = static_cast<int>(globalY - resizeStartMouseY_);
        int newX = resizeStartX_;
        int newY = resizeStartY_;
        int newW = resizeStartW_;
        int newH = resizeStartH_;
        constexpr int minW = 640;
        constexpr int minH = 480;
        if (windowResizeEdges_ & 1) { newW = resizeStartW_ - dx; newX = resizeStartX_ + dx; }
        if (windowResizeEdges_ & 2) { newW = resizeStartW_ + dx; }
        if (windowResizeEdges_ & 4) { newH = resizeStartH_ - dy; newY = resizeStartY_ + dy; }
        if (windowResizeEdges_ & 8) { newH = resizeStartH_ + dy; }
        if (newW < minW)
        {
            if (windowResizeEdges_ & 1)
                newX = resizeStartX_ + resizeStartW_ - minW;
            newW = minW;
        }
        if (newH < minH)
        {
            if (windowResizeEdges_ & 4)
                newY = resizeStartY_ + resizeStartH_ - minH;
            newH = minH;
        }
        SDL_SetWindowSize(window_, newW, newH);
        SDL_SetWindowPosition(window_, newX, newY);
    }

    // Live thumb dragging: recompute the scroll position from the cursor's
    // offset inside the thumb, clamped to the live favorite count.
    if (favoritesScrollDrag_ && activeTab_ == FrontendTab::Library)
    {

        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
        const SDL_FRect favInner{
            Dashboard(panels.rightContent).favorites.x + 5.0f,
            Dashboard(panels.rightContent).favorites.y + 28.0f,
            Dashboard(panels.rightContent).favorites.w - 10.0f,
            Dashboard(panels.rightContent).favorites.h - 33.0f
        };
        std::size_t favoriteCount = 0;
        for (const GameInfo& candidate : library_.Games())
            if (candidate.favorite) ++favoriteCount;
        const FavoritesScrollGeom geom = FavoritesScrollMetrics(favInner, favoriteCount);
        const float offset = (std::clamp)(event.y - favoritesScrollDragGrab_,
            geom.track.y, geom.track.y + geom.track.h - geom.thumb.h);
        const float freeH = (std::max)(1.0f, geom.track.h - geom.thumb.h);
        favoritesScroll_ = (std::clamp)(
            static_cast<int>((offset - geom.track.y) * static_cast<float>(geom.maxScroll) / freeH + 0.5f),
            0, geom.maxScroll);
        redraw_ = true;
    }

    // Game Library thumb drag: same Win95 semantics as the Favorites thumb.
    if (libraryListScrollDrag_ && activeTab_ == FrontendTab::Library)
    {
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window_, &w, &h);
        const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
        const SDL_FRect listRect = LibraryListRect(panels.leftContent);
        const int itemCount = static_cast<int>(collections_.Count());
        const FavoritesScrollGeom geom = LibraryListScrollMetrics(listRect, itemCount);
        const float offset = (std::clamp)(event.y - libraryListScrollDragGrab_,
            geom.track.y, geom.track.y + geom.track.h - geom.thumb.h);
        const float freeH = (std::max)(1.0f, geom.track.h - geom.thumb.h);
        libraryListScroll_ = (std::clamp)(
            static_cast<int>((offset - geom.track.y) * static_cast<float>(geom.maxScroll) / freeH + 0.5f),
            0, geom.maxScroll);
        redraw_ = true;
    }
}

SDL_FRect FrontendApp::QuickAddButtonRect(const SDL_FRect& importsPanel, int index) const
{
    // 70% of the original panel width, left-aligned; shared by the draw code
    // and the hit tests so the visible button and its click target can never
    // drift apart.
    return SDL_FRect{
        importsPanel.x + 10.0f,
        importsPanel.y + 29.0f + static_cast<float>(index) * 32.0f,
        (importsPanel.w - 20.0f) * 0.7f,
        27.0f};
}

SDL_FRect FrontendApp::LibraryOpenButtonRect(const SDL_FRect& foldersPanel, int index) const
{
    // Shared by the draw code and the hit paths so the visible OPEN button and
    // its click target can never drift apart (same convention as QuickAddButtonRect).
    return SDL_FRect{
        foldersPanel.x + foldersPanel.w - 74.0f,
        foldersPanel.y + 24.0f + static_cast<float>(index) * 28.0f,
        64.0f,
        24.0f};
}

std::filesystem::path FrontendApp::LibraryFolderPath(int index) const
{
    // Existing O2EM-NG runtime folder conventions, anchored at the runtime
    // base path owned by AssetManager (no second folder scheme, no absolute
    // development paths).
    const std::filesystem::path base = assetManager_.BasePath();
    static const char* const folders[] = {
        "ROMS", "BOXART", "SCREENSHOTS", "MANUALS", "BIOS"};
    return index >= 0 && index < 5 ? base / folders[index]
                                   : std::filesystem::path{};
}

FrontendApp::FavoritesScrollGeom FrontendApp::FavoritesScrollMetrics(
    const SDL_FRect& favInner, std::size_t favoriteCount)
{
    FavoritesScrollGeom geom;
    const int visibleRows = (std::max)(1, static_cast<int>((favInner.h - 12.0f) / 24.0f));
    const int maxScroll = static_cast<int>(favoriteCount) > visibleRows
        ? static_cast<int>(favoriteCount) - visibleRows
        : 0;
    // Clamp/reset whenever the favorite list shrinks (unfavorite, game
    // removal): the stored scroll may not exceed the live maximum.
    favoritesScroll_ = (std::clamp)(favoritesScroll_, 0, maxScroll);
    geom.visibleRows = visibleRows;
    geom.maxScroll = maxScroll;
    if (maxScroll <= 0)
        return geom; // everything fits: no scrollbar geometry at all

    // Win95 metrics: 17 px scrollbar column, 17 px arrows, sunken track.
    constexpr float kBarWidth = 17.0f;
    const float trackTop = favInner.y + 17.0f;
    const float trackBottom = favInner.y + favInner.h - 17.0f;
    geom.track = {favInner.x + favInner.w - kBarWidth, trackTop,
        kBarWidth, trackBottom - trackTop};
    geom.upArrow = {geom.track.x, favInner.y, kBarWidth, 17.0f};
    geom.downArrow = {geom.track.x, trackBottom, kBarWidth, 17.0f};

    // Proportional thumb, minimum one arrow tall, with the free space
    // distributed between the arrows (Win95-like behavior).
    const float trackH = geom.track.h;
    const float ratio = static_cast<float>(visibleRows) /
        static_cast<float>(visibleRows + maxScroll);
    const float thumbH = (std::max)(17.0f, trackH * ratio);
    const float freeH = (std::max)(0.0f, trackH - thumbH);
    const float thumbY = geom.track.y + (maxScroll > 0
        ? freeH * static_cast<float>(favoritesScroll_) / static_cast<float>(maxScroll)
        : 0.0f);
    geom.thumb = {geom.track.x, thumbY, geom.track.w, thumbH};
    return geom;
}

int FrontendApp::FavoritesScrollPartAt(float x, float y,
    const SDL_FRect& favInner, std::size_t favoriteCount)
{
    const FavoritesScrollGeom geom = FavoritesScrollMetrics(favInner, favoriteCount);
    if (geom.maxScroll <= 0)
        return 0;
    const auto contains = [](const SDL_FRect& r, float px, float py)
    {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    };
    if (contains(geom.upArrow, x, y)) return 1;
    if (contains(geom.downArrow, x, y)) return 2;
    if (contains(geom.track, x, y))
    {
        // Thumb containment first would be ambiguous at the thumb's edges;
        // explicit thumb rect keeps arrow/track/thumb disjoint.
        if (contains(geom.thumb, x, y)) return 5;
        return y < geom.thumb.y ? 3 : 4;
    }
    return 0;
}

bool FrontendApp::HandleFavoritesScrollbarAt(float x, float y,
    const SDL_FRect& favInner, std::size_t favoriteCount)
{
    const int part = FavoritesScrollPartAt(x, y, favInner, favoriteCount);
    if (part == 0)
        return false;
    const FavoritesScrollGeom geom = FavoritesScrollMetrics(favInner, favoriteCount);
    switch (part)
    {
    case 1: --favoritesScroll_; break;                      // up arrow
    case 2: ++favoritesScroll_; break;                      // down arrow
    case 3: favoritesScroll_ -= geom.visibleRows; break;    // page up
    case 4: favoritesScroll_ += geom.visibleRows; break;    // page down
    case 5: // thumb: begin drag, remember grab offset inside the thumb
        favoritesScrollDrag_ = true;
        favoritesScrollDragGrab_ = y - geom.thumb.y;
        break;
    default: break;
    }
    favoritesScroll_ = (std::clamp)(favoritesScroll_, 0, geom.maxScroll);
    redraw_ = true;
    return true;
}

// Game Library list viewport: same Win95 scrollbar idiom as Favorites, but
// with the list's 28 px rows and its own scroll state.
SDL_FRect FrontendApp::LibraryListRect(const SDL_FRect& content) const
{
    // Rows run from below the column header (content.y + 82) down to a small
    // inset above the panel bottom; derived entirely from the responsive
    // panel layout so windowed and fullscreen behave identically.
    return SDL_FRect{
        content.x + 10.0f,
        content.y + 82.0f,
        content.w - 20.0f,
        content.h - 92.0f};
}

FrontendApp::FavoritesScrollGeom FrontendApp::LibraryListScrollMetrics(
    const SDL_FRect& listRect, int itemCount)
{
    FavoritesScrollGeom geom;
    const int visibleRows = (std::max)(1,
        static_cast<int>(listRect.h / 28.0f));
    const int maxScroll = itemCount > visibleRows
        ? itemCount - visibleRows
        : 0;
    // Clamp/reset whenever the collection or view shrinks: the stored
    // scroll may not exceed the live maximum.
    libraryListScroll_ = (std::clamp)(libraryListScroll_, 0, maxScroll);
    geom.visibleRows = visibleRows;
    geom.maxScroll = maxScroll;
    if (maxScroll <= 0)
        return geom; // everything fits: no scrollbar geometry at all

    // Same Win95 metrics as the Favorites scrollbar: 17 px column, 17 px
    // arrows, sunken track, proportional thumb (minimum one arrow tall).
    constexpr float kBarWidth = 17.0f;
    const float trackTop = listRect.y + 17.0f;
    const float trackBottom = listRect.y + listRect.h - 17.0f;
    geom.track = {listRect.x + listRect.w - kBarWidth, trackTop,
        kBarWidth, trackBottom - trackTop};
    geom.upArrow = {geom.track.x, listRect.y, kBarWidth, 17.0f};
    geom.downArrow = {geom.track.x, trackBottom, kBarWidth, 17.0f};

    const float trackH = geom.track.h;
    const float ratio = static_cast<float>(visibleRows) /
        static_cast<float>(visibleRows + maxScroll);
    const float thumbH = (std::max)(17.0f, trackH * ratio);
    const float freeH = (std::max)(0.0f, trackH - thumbH);
    const float thumbY = geom.track.y + (maxScroll > 0
        ? freeH * static_cast<float>(libraryListScroll_) / static_cast<float>(maxScroll)
        : 0.0f);
    geom.thumb = {geom.track.x, thumbY, geom.track.w, thumbH};
    return geom;
}

int FrontendApp::LibraryListScrollPartAt(float x, float y,
    const SDL_FRect& listRect, int itemCount)
{
    const FavoritesScrollGeom geom = LibraryListScrollMetrics(listRect, itemCount);
    if (geom.maxScroll <= 0)
        return 0;
    const auto contains = [](const SDL_FRect& r, float px, float py)
    {
        return px >= r.x && px < r.x + r.w && py >= r.y && py < r.y + r.h;
    };
    if (contains(geom.upArrow, x, y)) return 1;
    if (contains(geom.downArrow, x, y)) return 2;
    if (contains(geom.track, x, y))
    {
        if (contains(geom.thumb, x, y)) return 5;
        return y < geom.thumb.y ? 3 : 4;
    }
    return 0;
}

bool FrontendApp::HandleLibraryListScrollbarAt(float x, float y,
    const SDL_FRect& listRect, int itemCount)
{
    const int part = LibraryListScrollPartAt(x, y, listRect, itemCount);
    if (part == 0)
        return false;
    const FavoritesScrollGeom geom = LibraryListScrollMetrics(listRect, itemCount);
    switch (part)
    {
    case 1: --libraryListScroll_; break;                    // up arrow
    case 2: ++libraryListScroll_; break;                    // down arrow
    case 3: libraryListScroll_ -= geom.visibleRows; break;  // page up
    case 4: libraryListScroll_ += geom.visibleRows; break;  // page down
    case 5: // thumb: begin drag, remember grab offset inside the thumb
        libraryListScrollDrag_ = true;
        libraryListScrollDragGrab_ = y - geom.thumb.y;
        break;
    default: break;
    }
    libraryListScroll_ = (std::clamp)(libraryListScroll_, 0, geom.maxScroll);
    redraw_ = true;
    return true;
}

void FrontendApp::KeepLibrarySelectionVisible()
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
    const SDL_FRect listRect = LibraryListRect(panels.leftContent);
    const int itemCount = static_cast<int>(collections_.Count());
    const FavoritesScrollGeom geom = LibraryListScrollMetrics(listRect, itemCount);
    if (geom.maxScroll <= 0)
    {
        libraryListScroll_ = 0;
        return;
    }
    const int selected = static_cast<int>(collections_.CurrentPosition());
    if (selected < libraryListScroll_)
        libraryListScroll_ = selected;
    else if (selected >= libraryListScroll_ + geom.visibleRows)
        libraryListScroll_ = selected - geom.visibleRows + 1;
    libraryListScroll_ = (std::clamp)(libraryListScroll_, 0, geom.maxScroll);
}

bool FrontendApp::TryLibraryQuickControlAt(float x, float y)
{
    int w = 0;
    int h = 0;
    SDL_GetWindowSize(window_, &w, &h);
    const FrontendPanelLayout panels = FrontendPanels_Calculate(w, h);
    const auto layout=Dashboard(panels.rightContent);
    const auto& quickFrame=layout.quick;
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
        inner.x + 4.0f, inner.y + 2.0f,
        (inner.w - 8.0f) * 0.75f, 48.0f
    };
    const SDL_FRect regionHit{
        inner.x + 4.0f, inner.y + 48.0f,
        (inner.w - 8.0f) * 0.75f, 48.0f
    };
    const SDL_FRect scanlinesHit{
        inner.x + 4.0f, inner.y + 94.0f,
        inner.w - 8.0f, 26.0f
    };
    // Fullscreen checkbox row, directly below Scanlines (drawn at
    // quickInner.y + 122). Same authoritative setting as the Settings screen:
    // toggled via ActivateSettingsSelection() so both UIs stay in sync.
    const SDL_FRect fullscreenHit{
        inner.x + 4.0f, inner.y + 118.0f,
        inner.w - 8.0f, 28.0f
    };

    const auto contains = [x, y](const SDL_FRect& rect)
    {
        return x >= rect.x && x < rect.x + rect.w &&
               y >= rect.y && y < rect.y + rect.h;
    };

    // If a Quick Settings dropdown is already open, resolve the selected item first.
    if (openDropdown_ == 3)
    {
        RefreshInstalledBiosFiles();
        const float itemH = 25.0f;
        const SDL_FRect popup{inner.x + 10.0f, inner.y + 46.0f,
            (inner.w - 20.0f) * 0.75f, itemH * static_cast<float>(installedBiosFiles_.size())};
        if (contains(popup) && !installedBiosFiles_.empty())
        {
            const int item = static_cast<int>((y - popup.y) / itemH);
            if (item >= 0 && item < static_cast<int>(installedBiosFiles_.size()))
                settings_.bios_file = installedBiosFiles_[item];
            SaveSettings(settingsPath_, settings_); openDropdown_ = -1; redraw_ = true; return true;
        }
        openDropdown_ = -1;
        redraw_ = true;
        return true;
    }
    else if (openDropdown_ == 1)
    {
        const float itemH = 25.0f;
        const SDL_FRect popup{inner.x + 10.0f, inner.y + 92.0f, (inner.w - 20.0f) * 0.75f, itemH * 3.0f};
        if (contains(popup))
        {
            const int item = static_cast<int>((y - popup.y) / itemH);
            settings_.region_mode = item == 0 ? RegionMode::Auto : (item == 1 ? RegionMode::PAL : RegionMode::NTSC);
            SaveSettings(settingsPath_, settings_); openDropdown_ = -1; redraw_ = true; return true;
        }
        openDropdown_ = -1;
        redraw_ = true;
        return true;
    }

    const SDL_FRect scaleHit{layout.cover.x+5,layout.cover.y+layout.cover.h-31,layout.cover.w-10,28};    if (contains(scaleHit)) { scaleCoverImage_=!scaleCoverImage_; redraw_=true; return true; }
    for(int i=0;i<3;++i)
    {
        const SDL_FRect button=QuickAddButtonRect(layout.imports,i);
        if(contains(button))
        {
            quickAddPressed_ = i;
            redraw_ = true;
            return true;
        }
    }
    // Library Folders OPEN buttons: arm the pressed state on mouse-down;
    // HandleMouseButtonUp performs the action on release inside the rect.
    {
        const auto dashboardLayout=Dashboard(panels.rightContent);
        if(dashboardLayout.hasRightColumn)
        {
            for(int i=0;i<5;++i)
            {
                if(contains(LibraryOpenButtonRect(dashboardLayout.folders,i)))
                {
                    folderOpenPressed_ = i;
                    redraw_ = true;
                    return true;
                }
            }
        }
    }
    if (contains(biosHit)) { settingsSelected_ = 3; openDropdown_ = 3; redraw_ = true; return true; }
    if (contains(regionHit)) { settingsSelected_ = 1; openDropdown_ = 1; redraw_ = true; return true; }

    if (contains(scanlinesHit))
    {
        settingsSelected_ = 2;
        ActivateSettingsSelection();
        return true;
    }

    if (contains(fullscreenHit))
    {
        settingsSelected_ = 0;
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
    const auto layout=Dashboard(panels.rightContent);
    const bool recentHit=x>=layout.recent.x && x<layout.recent.x+layout.recent.w &&
        y>=layout.recent.y && y<layout.recent.y+layout.recent.h;
    const auto& favoritesFrame=recentHit ? layout.recent : layout.favorites;
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
        if (recentHit ? candidate.lastPlayed>0 : candidate.favorite) favorites.push_back(&candidate);
    std::stable_sort(favorites.begin(), favorites.end(), [recentHit](const GameInfo* a, const GameInfo* b)
    {
        return recentHit ? a->lastPlayed > b->lastPlayed : a->title < b->title;
    });

    if (recentHit)
    {
        const int maximumRows = 4;
        const float rowTop = favoritesFrame.y + 28.0f;
        if (y < rowTop) return false;
        const int row = static_cast<int>((y - rowTop) / 24.0f);
        const int shown = (std::min)(maximumRows, static_cast<int>(favorites.size()));
        if (row < 0 || row >= shown)
            return false;
        if (!collections_.SelectFilename(favorites[static_cast<std::size_t>(row)]->filename))
            return false;
    }
    else
    {
        // Scrollbar column: let the scrollbar handlers own clicks there.
        // While a thumb drag is in progress the mouse may hover anywhere -
        // row picking must not swallow the motion-driven updates.
        if (!favoritesScrollDrag_ &&
            FavoritesScrollPartAt(x, y, inner, favorites.size()) != 0)
        {
            return HandleFavoritesScrollbarAt(x, y, inner, favorites.size());
        }
        const FavoritesScrollGeom geom =
            FavoritesScrollMetrics(inner, favorites.size());
        const float rowTop = inner.y + 9.0f;
        if (y < rowTop) return false;
        const int row = static_cast<int>((y - rowTop) / 24.0f);
        const int first = favoritesScroll_;
        const int shown = (std::min)(geom.visibleRows,
            static_cast<int>(favorites.size()) - first);
        if (row < 0 || row >= shown)
            return false;
        if (!collections_.SelectFilename(
                favorites[static_cast<std::size_t>(first + row)]->filename))
            return false;
    }

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

    const auto descriptionFrame=Dashboard(panels.rightContent).description;

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
    case 0: return &metadataWorkingCopy_.catalogId;
    case 1: return &metadataWorkingCopy_.title;
    case 2: return &metadataWorkingCopy_.year;
    case 3: return &metadataWorkingCopy_.publisher;
    case 4: return &metadataWorkingCopy_.developer;
    case 5: return &metadataWorkingCopy_.genre;
    case 6: return &metadataWorkingCopy_.players;
    case 7: return &metadataWorkingCopy_.controls;
    case 8: return &metadataWorkingCopy_.voiceModule;
    case 9: return &metadataWorkingCopy_.videopacPlus;
    case 10: return &metadataWorkingCopy_.rating;
    case 11: return &metadataWorkingCopy_.shortDescription;
    case 12: return &metadataWorkingCopy_.description;
    case 13: return &metadataWorkingCopy_.trivia;
    case 14: return &metadataManualPath_;
    default: return nullptr;
    }
}

const char* FrontendApp::CurrentMetadataLabel() const
{
    static const char* labels[MetadataFieldCount] = {
        "Catalog ID", "Title", "Year", "Publisher", "Developer", "Genre", "Players",
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

namespace
{
    std::wstring CatalogUtf8ToWide(const std::string& text)
    {
        if (text.empty()) return {};
        const int count = MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
            static_cast<int>(text.size()), nullptr, 0);
        std::wstring wide(static_cast<std::size_t>((std::max)(0, count)), L'\0');
        if (!wide.empty())
            MultiByteToWideChar(CP_UTF8, 0, text.c_str(),
                static_cast<int>(text.size()), wide.data(), count);
        return wide;
    }
}

// Manual DELETE GAME DATA (Patch 0031): a scalpel for genuinely unwanted
// database/catalogue entries. Deletes EXACTLY the selected entry's database
// row (by its exact rom_filename key, so 59 and 59+ stay independent) plus
// a suppression marker so catalogue seeding cannot silently recreate the
// entry. It NEVER deletes ROM files, BIOS/firmware, covers/box art,
// screenshots/GIF/MP4, manuals or any media directory - files are not
// touched at all. Counterpart entries (N vs N+) are unaffected.
void FrontendApp::DeleteGameDataForSelectedGame()
{
    const GameInfo* game = GetSelectedGame();
    if (!game || game->filename.empty())
        return;

    const std::string idText = DisplayCatalogId(game);
    const std::string nameText = game->title.empty() ? game->filename : game->title;
    const bool romInstalled = !game->romPath.empty() || !game->rom.path.empty();
    std::wstring message =
        L"Delete game data for catalogue entry " +
        CatalogUtf8ToWide(idText.empty() ? nameText : idText) +
        L" (" + CatalogUtf8ToWide(nameText) + L")?\n\n"
        L"This removes the database entry only.\n"
        L"ROM files and media will NOT be deleted.";
    if (romInstalled)
        message +=
            L"\n\nNote: the installed ROM stays in the library as a basic entry "
            L"with default data.";
    const int choice = MessageBoxW(nullptr, message.c_str(),
        L"O2EM-NG - Delete Game Data",
        MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON2);
    if (choice != IDYES)
        return;

    const std::string deletedFilename = game->filename;
    const std::size_t deletedPosition = collections_.CurrentPosition();
    const bool rowDeleted = gameDatabase_.DeleteGameRecord(deletedFilename);
    const bool suppressed = gameDatabase_.SuppressCatalogEntry(deletedFilename);

    if (rowDeleted || suppressed)
    {
        importStatus_ = "Game Data deleted: " + deletedFilename;

        // Rebuild from the authoritative ROM directory (same sequence as the
        // ROM-deletion path). The suppression marker keeps the entry out of
        // catalogue seeding; installed ROMs are unaffected.
        const std::vector<RomEntry> installedRoms =
            LoadRoms((importManager_.BasePath() / "ROMS").string());
        library_.SetGames(installedRoms);
        metadataEngine_.Populate(library_);
        gameDatabase_.InitializeAndPopulate(library_);
        for (GameInfo& remainingGame : library_.Games())
            ApplyCatalogFallbacks(remainingGame);
        assetManager_.Populate(library_);
        collections_.Attach(&library_);

        // Keep the selection valid near where the deleted entry was.
        if (collections_.Count() == 0)
        {
            libraryListScroll_ = 0;
        }
        else
        {
            const std::size_t clamped =
                (std::min)(deletedPosition, collections_.Count() - 1);
            collections_.SelectPosition(clamped);
            KeepLibrarySelectionVisible();
        }

        FrontendBoxArt_Shutdown();
        ManualPreview_Shutdown();
        FrontendScreenshot_Invalidate();
    }
    else
    {
        importStatus_ = "Could not delete Game Data for " + deletedFilename;
    }
    redraw_ = true;
}

void FrontendApp::SaveMetadataEdit()
{
    GameInfo* game = GetSelectedGame();
    if (!game) return;
    if (metadataTextInput_) SDL_StopTextInput(window_);
    metadataTextInput_ = false;

    metadataWorkingCopy_.catalogId = TrimCatalogId(metadataWorkingCopy_.catalogId);

    if (!metadataManualPath_.empty())
    {
        std::filesystem::path path(metadataManualPath_);
        metadataWorkingCopy_.manual = path.is_absolute() ? path : gameDatabase_.BasePath() / path;
    }
    else
        metadataWorkingCopy_.manual.clear();

    const std::string oldFilename = game->filename;
    const std::filesystem::path oldRomPath = game->romPath;
    const std::string canonicalFilename =
        CanonicalRomFilenameForCatalogId(metadataWorkingCopy_.catalogId);

    if (!gameDatabase_.SaveUserMetadata(metadataWorkingCopy_))
    {
        std::printf("O2EM-NG: failed to save metadata for %s.\n", oldFilename.c_str());
        redraw_ = true;
        return;
    }

    bool renamed = false;
    std::filesystem::path newRomPath = oldRomPath;

    if (!canonicalFilename.empty() &&
        !oldRomPath.empty() &&
        _stricmp(oldFilename.c_str(), canonicalFilename.c_str()) != 0)
    {
        newRomPath = oldRomPath.parent_path() / canonicalFilename;

        std::error_code error;
        const bool destinationExists = std::filesystem::exists(newRomPath, error) && !error;
        if (destinationExists)
        {
            char message[1024]{};
            std::snprintf(message, sizeof(message),
                "The Catalog ID was saved, but the ROM was not renamed.\n\n"
                "Destination already exists:\n%s\n\n"
                "O2EM-NG will never overwrite an existing ROM.",
                newRomPath.string().c_str());

            HWND owner = static_cast<HWND>(SDL_GetPointerProperty(
                SDL_GetWindowProperties(window_),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
            MessageBoxA(owner, message, "O2EM-NG - ROM rename conflict",
                MB_OK | MB_ICONWARNING);
            std::printf("O2EM-NG: ROM rename skipped; destination already exists: %s\n",
                newRomPath.string().c_str());
        }
        else
        {
            error.clear();
            std::filesystem::rename(oldRomPath, newRomPath, error);
            if (!error)
            {
                if (gameDatabase_.RenameRomFilename(oldFilename, canonicalFilename))
                {
                    renamed = true;
                    metadataWorkingCopy_.filename = canonicalFilename;
                    metadataWorkingCopy_.romPath = newRomPath;
                    metadataWorkingCopy_.rom.name = canonicalFilename;
                    metadataWorkingCopy_.rom.path = newRomPath.string();
                    metadataWorkingCopy_.rom.info = ClassifyRom(newRomPath);

                    std::printf("O2EM-NG: ROM renamed: %s -> %s\n",
                        oldFilename.c_str(), canonicalFilename.c_str());
                }
                else
                {
                    std::error_code rollbackError;
                    std::filesystem::rename(newRomPath, oldRomPath, rollbackError);

                    HWND owner = static_cast<HWND>(SDL_GetPointerProperty(
                        SDL_GetWindowProperties(window_),
                        SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
                    MessageBoxA(owner,
                        "The Catalog ID was saved, but the ROM filename could not be "
                        "updated in the database.\n\nThe ROM rename has been rolled back.",
                        "O2EM-NG - ROM rename failed", MB_OK | MB_ICONWARNING);
                    std::printf("O2EM-NG: database ROM-key rename failed; filesystem rename rolled back.\n");
                }
            }
            else
            {
                char message[1024]{};
                std::snprintf(message, sizeof(message),
                    "The Catalog ID was saved, but Windows could not rename the ROM.\n\n"
                    "%s\n\nError code: %d",
                    oldRomPath.string().c_str(), error.value());

                HWND owner = static_cast<HWND>(SDL_GetPointerProperty(
                    SDL_GetWindowProperties(window_),
                    SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
                MessageBoxA(owner, message, "O2EM-NG - ROM rename failed",
                    MB_OK | MB_ICONWARNING);
                std::printf("O2EM-NG: ROM rename failed (%d): %s\n",
                    error.value(), oldRomPath.string().c_str());
            }
        }
    }

    *game = metadataWorkingCopy_;

    if (renamed)
        metadataWorkingCopy_ = *game;

    collections_.Rebuild();
    metadataEditMode_ = false;
    std::printf("O2EM-NG: metadata saved for %s.\n", game->filename.c_str());
    redraw_ = true;
}

bool FrontendApp::IsLongMetadataField() const noexcept
{
    return metadataSelected_ >= 11 && metadataSelected_ <= 13;
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

SDL_FRect FrontendApp::MetadataButtonRect(const SDL_FRect& inner, int index) const
{
    // Shared by the draw and hit paths so hitboxes always match what is
    // rendered. index 0 = EDIT GAME DATA (view mode, bottom right);
    // index 1 = SAVE, 2 = CANCEL (edit mode, bottom left);
    // index 3 = DELETE GAME DATA (view mode, bottom left).
    const float buttonY = inner.y + inner.h - 44.0f;
    const float buttonH = 32.0f;
    switch (index)
    {
    case 0:
        return {inner.x + inner.w - 192.0f, buttonY, 178.0f, buttonH};
    case 1:
        return {inner.x + 24.0f, buttonY, 96.0f, buttonH};
    case 3:
        // DELETE GAME DATA (view mode, bottom left).
        return {inner.x + 24.0f, buttonY, 168.0f, buttonH};
    default:
        return {inner.x + 132.0f, buttonY, 96.0f, buttonH};
    }
}

void FrontendApp::DrawWin95Button(const SDL_FRect& rect, const char* label,
    bool pressed)
{
    if (!pressed)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        const SDL_FRect buttonShadow{rect.x + 3.0f, rect.y + 3.0f, rect.w, rect.h};
        SDL_RenderFillRect(renderer_, &buttonShadow);
    }
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Face);
    SDL_RenderFillRect(renderer_, &rect);
    if (pressed)
    {
        // Same pressed-edge idiom as the Quick Add buttons: dark top/left,
        // bright bottom/right.
        Win95Theme::SetRenderColor(renderer_, Win95Theme::DarkShadow);
        SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer_, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer_, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }
    else
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Highlight);
        SDL_RenderLine(renderer_, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer_, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Shadow);
        SDL_RenderLine(renderer_, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer_, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }
    // ~8px per character at 0.95 scale (the same estimate the caret math
    // uses); centers short labels without a text-measurement API.
    const float pressOffset = pressed ? 2.0f : 0.0f;
    const float textScale = 0.95f;
    const float textWidth = 8.0f * textScale * static_cast<float>(std::strlen(label));
    // The label of an active Win95 push button is always black. The raised
    // -edge drawing above leaves Shadow as the current color, which made the
    // label inherit grey and read as disabled (OPEN buttons). Explicitly
    // restore WindowText here so every button using this helper (Game Data,
    // DELETE GAME DATA, Library OPEN) renders with active-button text.
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_,
        rect.x + (std::max)(2.0f, (rect.w - textWidth) * 0.5f) + pressOffset,
        rect.y + (rect.h - 16.0f) * 0.5f + pressOffset,
        textScale, label);
}

void FrontendApp::DrawMetadataButton(const SDL_FRect& rect, int index,
    const char* label)
{
    DrawWin95Button(rect, label, metadataButtonPressed_ == index);
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
        // EDIT GAME DATA / DELETE GAME DATA buttons: arm the pressed state
        // on mouse-down; the action fires on mouse-up inside the same rect
        // (Win95 push button). Same frame/inner math as DrawGameInformationTab
        // (margin 14 + 4).
        const SDL_FRect inner{c.x + 18.0f, c.y + 18.0f, c.w - 36.0f, c.h - 36.0f};
        for (const int index : {3, 0})
        {
            const SDL_FRect button = MetadataButtonRect(inner, index);
            if (x >= button.x && x < button.x + button.w &&
                y >= button.y && y < button.y + button.h)
            {
                metadataButtonPressed_ = index;
                redraw_ = true;
                return true;
            }
        }
        return false;
    }
    const float firstY=c.y+58.0f;
    for(int i=0;i<MetadataFieldCount;++i)
    {
        float ry=firstY+i*29.0f;
        if(x>=c.x+20 && x<c.x+c.w-20 && y>=ry-5 && y<ry+22)
        { metadataSelected_=i; redraw_=true; if(i!=14) EditCurrentMetadataField(); return true; }
    }
    // SAVE (1) / CANCEL (2) push buttons: arm on mouse-down, confirm on
    // mouse-up inside the same rect. Same inner math as the draw path.
    {
        const SDL_FRect inner{c.x + 18.0f, c.y + 18.0f, c.w - 36.0f, c.h - 36.0f};
        for (int index = 1; index <= 2; ++index)
        {
            const SDL_FRect button = MetadataButtonRect(inner, index);
            if (x >= button.x && x < button.x + button.w &&
                y >= button.y && y < button.y + button.h)
            {
                metadataButtonPressed_ = index;
                redraw_ = true;
                return true;
            }
        }
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
            {
                std::string filename = entry.path().filename().string();
                std::string lowerName = filename;
                std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(),
                    [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

                // C7010/C7420 NSC800 firmware is expansion-module firmware,
                // not a selectable G7000/G7400/Jopac console BIOS.
                if (lowerName.rfind("c7010_", 0) == 0 || lowerName.rfind("c7420_", 0) == 0)
                    continue;

                installedBiosFiles_.push_back(filename);
            }
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

    // Patch 0030A Settings fix: expansion-module firmware lives in BIOS
    // subfolders and must not be mixed with the selectable console BIOS list.
    // Detect it separately so Settings can report both C7010 and C7420.
    error.clear();
    c7010FirmwareInstalled_ = std::filesystem::is_regular_file(
        folder / "C7010" / "c7010_z80.bin", error) && !error;
    error.clear();
    c7420FirmwareInstalled_ = std::filesystem::is_regular_file(
        folder / "C7420" / "c7420_z80.bin", error) && !error;
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
    if (type != ImportAssetType::Bios && type != ImportAssetType::Rom && !game)
    {
        importStatus_ = "Select a game before importing this file.";
        redraw_ = true;
        return;
    }

    // Patch 0024: ROM import is independent of the selected catalogue row.
    // The source filename is preserved and unknown ROMs become library items.

    const ImportResult result = (type == ImportAssetType::Bios)
        ? importManager_.ImportBios()
        : (type == ImportAssetType::Rom
            ? importManager_.ImportRom()
            : importManager_.ImportForGame(type, *game));
    importStatus_ = result.message;
    if (result.success)
    {
        if (type == ImportAssetType::Bios)
        {
            RefreshInstalledBiosFiles(false);
            settings_.bios_file = result.destination.filename().string();
            SaveSettings(settingsPath_, settings_);
        }
        if (type == ImportAssetType::Rom)
        {
            const std::vector<RomEntry> installedRoms =
                LoadRoms((importManager_.BasePath() / "ROMS").string());
            library_.SetGames(installedRoms);
            metadataEngine_.Populate(library_);
            gameDatabase_.InitializeAndPopulate(library_);
            for (GameInfo& importedGame : library_.Games())
                ApplyCatalogFallbacks(importedGame);
            // Phase 4B: resolve assets only after Catalog IDs/user metadata
            // have been restored from the database.
            assetManager_.Populate(library_);
            FrontendBoxArt_Shutdown();
            ManualPreview_Shutdown();
            FrontendScreenshot_Invalidate();
            collections_.Attach(&library_);
        }
        else if (game)
        {
            RefreshSelectedGameAssets(*game);
        }
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

    if (type == ImportAssetType::Screenshot) FrontendScreenshot_Invalidate();

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
                // Safe Delete: deleting the ROM never silently deletes the
                // user's database metadata. The ROM has already been moved to
                // the Recycle Bin; now offer a separate, explicit database
                // cleanup choice. "No" is the safe/default action.
                const std::string deletedRomFilename = game->filename;
                const std::wstring dataQuestion =
                    L"The ROM was moved to the Recycle Bin.\n\n"
                    L"Also delete this game's stored Game Data from the O2EM-NG database?\n\n"
                    L"Choose No to keep metadata for later re-import.";
                const int dataChoice = MessageBoxW(nullptr, dataQuestion.c_str(),
                    L"O2EM-NG - Safe Delete",
                    MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);

                if (dataChoice == IDYES)
                {
                    if (gameDatabase_.DeleteGameRecord(deletedRomFilename))
                        importStatus_ = "ROM moved to Recycle Bin; stored Game Data deleted.";
                    else
                        importStatus_ = "ROM moved to Recycle Bin, but Game Data could not be deleted.";
                }
                else
                {
                    importStatus_ = "ROM moved to Recycle Bin; stored Game Data kept.";
                }

                // Rebuild the catalogue immediately after ROM deletion.
                // Official catalogue rows remain as uninstalled entries; an
                // unknown ROM disappears from the visible library, while its
                // database row remains intact unless explicitly deleted above.
                const std::vector<RomEntry> installedRoms =
                    LoadRoms((importManager_.BasePath() / "ROMS").string());
                library_.SetGames(installedRoms);
                metadataEngine_.Populate(library_);
                gameDatabase_.InitializeAndPopulate(library_);
                for (GameInfo& remainingGame : library_.Games())
                    ApplyCatalogFallbacks(remainingGame);
                // Phase 4B: database/user identity first, media resolution last.
                assetManager_.Populate(library_);
                collections_.Attach(&library_);

                FrontendBoxArt_Shutdown();
                ManualPreview_Shutdown();
                FrontendScreenshot_Invalidate();
            }
            else
            {
                RefreshSelectedGameAssets(*game);
            }
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

    const std::string idText = !source->catalogId.empty()
        ? source->catalogId
        : (source->videopacNumber > 0
            ? (source->videopacNumber < 10 ? "0" + std::to_string(source->videopacNumber) : std::to_string(source->videopacNumber))
            : "-");

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
        source->catalogId,
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
        "Catalog ID", "Title", "Year", "Publisher", "Developer", "Genre", "Players",
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
            Win95Theme::SetRenderColor(renderer_, Win95Theme::SelectedItem);
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
        DrawMetadataButton(MetadataButtonRect(inner, 1), 1, "SAVE");
        DrawMetadataButton(MetadataButtonRect(inner, 2), 2, "CANCEL");
        // ENTER toggles in-place typing; pressing ENTER again on the long
        // Description/Trivia fields opens the popup full editor.
        DrawText(
            renderer_,
            inner.x + 242.0f,
            inner.y + inner.h - 38.0f,
            1.1f,
            "ENTER: Edit selected field   Description / Trivia: ENTER opens full editor");
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
        DrawMetadataButton(MetadataButtonRect(inner, 3), 3, "DELETE GAME DATA");
        DrawMetadataButton(MetadataButtonRect(inner, 0), 0, "EDIT GAME DATA");
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

    // Bottom-right version stamp. Fixed UI chrome, independent of the
    // editable database project pages, so it always shows the compiled-in
    // application version.
    const float margin = 14.0f;
    const SDL_FRect frame{content.x + margin, content.y + margin,
        content.w - margin * 2.0f, content.h - margin * 2.0f};
    const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f, frame.w - 8.0f, frame.h - 8.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, inner.x + inner.w - 170.0f, inner.y + inner.h - 19.0f, 0.8f,
        std::string("O2EM-NG ") + O2emVersion::kAppVersion);
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
    const int pageCount = (std::max)(1, static_cast<int>(projectPages_.size()));
    const float buttonWidth = (inner.w - 24.0f - buttonGap * (pageCount - 1)) / static_cast<float>(pageCount);
    for (int i = 0; i < static_cast<int>(projectPages_.size()); ++i)
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
        const bool specialName = page.pageKey == "special_thanks" &&
            (lines[i].rfind("Mark Guttenbrunner", 0) == 0 || lines[i].rfind("Brian Dehli", 0) == 0);
        DrawText(renderer_, textArea.x + 4.0f, y, 0.92f, lines[i]);
        if (specialName) DrawText(renderer_, textArea.x + 5.0f, y, 0.92f, lines[i]);
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
    const SDL_FRect frame{content.x + margin, content.y + margin, content.w - margin * 2.0f, content.h - margin * 2.0f};
    DrawSunkenFrame(renderer_, frame);
    const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f, frame.w - 8.0f, frame.h - 8.0f};
    Win95Theme::SetRenderColor(renderer_, Win95Theme::Window); SDL_RenderFillRect(renderer_, &inner);
    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, inner.x + 20.0f, inner.y + 18.0f, 1.5f, "EMULATOR SETTINGS");

    const float x = inner.x + 34.0f;
    auto drawCheck = [&](float y, const char* label, bool checked)
    {
        SDL_FRect box{x, y, 19.0f, 19.0f}; DrawSunkenFrame(renderer_, box);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window); SDL_FRect f{box.x+2,box.y+2,box.w-4,box.h-4}; SDL_RenderFillRect(renderer_, &f);
        if (checked) { Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText); DrawText(renderer_, box.x+3, box.y-1, .9f, "x"); }
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText); DrawText(renderer_, box.x+29, y-1, 1.05f, label);
    };
    auto drawCombo = [&](float y, const char* label, const std::string& value)
    {
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText); DrawText(renderer_, x, y, 1.0f, label);
        SDL_FRect box{x, y+23.0f, (std::min)(430.0f, inner.w-90.0f), 30.0f}; DrawSunkenFrame(renderer_, box);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Window); SDL_FRect f{box.x+2,box.y+2,box.w-30,box.h-4}; SDL_RenderFillRect(renderer_, &f);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText); DrawText(renderer_, f.x+6,f.y+3,.95f,value);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::Face); SDL_FRect a{box.x+box.w-28,box.y+2,26,box.h-4}; SDL_RenderFillRect(renderer_,&a);
        Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
        DrawText(renderer_,a.x+8,a.y+3,.9f,"v");
    };

    drawCheck(inner.y + 76.0f, "Fullscreen (changes immediately)", settings_.start_fullscreen);
    std::string region = RegionModeToString(settings_.region_mode); std::transform(region.begin(),region.end(),region.begin(),[](unsigned char c){return static_cast<char>(std::toupper(c));});
    drawCombo(inner.y + 126.0f, "Region Mode", region);
    drawCheck(inner.y + 205.0f, "Scanlines", settings_.scanlines);
    drawCombo(inner.y + 255.0f, "BIOS File", settings_.bios_file.empty()?"No BIOS installed":settings_.bios_file);

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, x, inner.y + 312.0f, 0.95f,
        std::string("Current version: ") + O2emVersion::kAppVersion);

    // Updates section (Phase 2: manual check only). The CHECK UPDATES
    // button reuses DrawWin95Button with a dedicated pressed flag, exactly
    // like the Library OPEN buttons (arm on down, act on release).
    {
        const SDL_FRect updatesButton{x, inner.y + 412.0f, 170.0f, 32.0f};
        DrawWin95Button(updatesButton, "CHECK UPDATES", updateCheckPressed_);
        const bool updateAvailableNow =
            updateCheck_.GetState() == UpdateCheck::State::UpdateAvailable;
        if (updateAvailableNow)
        {
            const SDL_FRect viewRelease{x + 190.0f, inner.y + 412.0f, 150.0f, 32.0f};
            DrawWin95Button(viewRelease, "VIEW RELEASE", viewReleasePressed_);
        }
        const char* statusText = "Not checked";
        const SDL_Color* statusColor = &Win95Theme::WindowText;
        std::string statusDetail;
        switch (updateCheck_.GetState())
        {
        case UpdateCheck::State::Idle:
            statusText = "Not checked";
            break;
        case UpdateCheck::State::Checking:
            statusText = "Checking...";
            break;
        case UpdateCheck::State::UpToDate:
            statusText = "Up to date";
            break;
        case UpdateCheck::State::UpdateAvailable:
            statusText = "New version available";
            statusDetail = updateCheck_.GetLatestVersion();
            break;
        case UpdateCheck::State::Error:
            statusText = "Could not check for updates";
            statusDetail = updateCheck_.GetErrorMessage();
            statusColor = &Win95Theme::Shadow;
            break;
        }
        Win95Theme::SetRenderColor(renderer_, *statusColor);
        DrawText(renderer_, x, inner.y + 456.0f, 0.95f, statusText);
        if (!statusDetail.empty())
            DrawText(renderer_,
                x + 8.0f * 0.95f * static_cast<float>(std::strlen(statusText)) + 10.0f,
                inner.y + 456.0f, 0.95f, statusDetail.c_str());
    }

    Win95Theme::SetRenderColor(renderer_, Win95Theme::WindowText);
    DrawText(renderer_, x, inner.y + 345.0f, 1.0f, std::string("C7010 NSC800 BIOS: ") + (c7010FirmwareInstalled_ ? "c7010_z80.bin  [INSTALLED]" : "NOT FOUND"));
    DrawText(renderer_, x, inner.y + 374.0f, 1.0f, std::string("C7420 NSC800 BIOS: ") + (c7420FirmwareInstalled_ ? "c7420_z80.bin  [INSTALLED]" : "NOT FOUND"));

    if (openDropdown_ == 1)
    {
        const char* items[]={"AUTO","PAL","NTSC"}; const float ih=30.0f; SDL_FRect pop{x,inner.y+179.0f,(std::min)(430.0f,inner.w-90.0f),ih*3};
        Win95Theme::SetRenderColor(renderer_,Win95Theme::Window); SDL_RenderFillRect(renderer_,&pop); DrawSunkenFrame(renderer_,pop);
        const int selectedRegion = settings_.region_mode == RegionMode::Auto ? 0 : (settings_.region_mode == RegionMode::PAL ? 1 : 2);
        for(int i=0;i<3;++i)
        {
            SDL_FRect row{pop.x+2.0f,pop.y+2.0f+i*ih,pop.w-4.0f,ih};
            Win95Theme::SetRenderColor(renderer_,i==selectedRegion?Win95Theme::SelectedItem:Win95Theme::Window); SDL_RenderFillRect(renderer_,&row);
            Win95Theme::SetRenderColor(renderer_,i==selectedRegion?Win95Theme::SelectedItemText:Win95Theme::WindowText);
            DrawText(renderer_,pop.x+7,pop.y+4+i*ih,.95f,items[i]);
        }
    }
    else if (openDropdown_ == 3)
    {
        RefreshInstalledBiosFiles(); const float ih=30.0f; SDL_FRect pop{x,inner.y+308.0f,(std::min)(430.0f,inner.w-90.0f),ih*static_cast<float>(installedBiosFiles_.size())};
        Win95Theme::SetRenderColor(renderer_,Win95Theme::Window); SDL_RenderFillRect(renderer_,&pop); DrawSunkenFrame(renderer_,pop);
        for(int i=0;i<static_cast<int>(installedBiosFiles_.size());++i)
        {
            const bool selected=installedBiosFiles_[i]==settings_.bios_file;
            SDL_FRect row{pop.x+2.0f,pop.y+2.0f+i*ih,pop.w-4.0f,ih};
            Win95Theme::SetRenderColor(renderer_,selected?Win95Theme::SelectedItem:Win95Theme::Window); SDL_RenderFillRect(renderer_,&row);
            Win95Theme::SetRenderColor(renderer_,selected?Win95Theme::SelectedItemText:Win95Theme::WindowText);
            DrawText(renderer_,pop.x+7,pop.y+4+i*ih,.95f,installedBiosFiles_[i]);
        }
    }
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
