#include <SDL3/SDL.h>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdio>
#include "settings.h"
#include "vdc_stub.h"
#include "rom_browser.h"
#include "launcher.h"
#include "input_manager.h"

static constexpr int FRONTEND_WIDTH = 1200;
static constexpr int FRONTEND_HEIGHT = 900;
static constexpr int VISIBLE_ROWS = 18;
static constexpr Sint16 STICK_DEADZONE = 16000;

static void DrawText(SDL_Renderer* renderer, float x, float y,
                     float scale, const std::string& text)
{
    SDL_SetRenderScale(renderer, scale, scale);
    SDL_RenderDebugText(renderer, x / scale, y / scale, text.c_str());
    SDL_SetRenderScale(renderer, 1.0f, 1.0f);
}

static int GetMenuItemCount(const std::vector<RomEntry>& roms)
{
    // ROMs + SETTINGS entry
    return static_cast<int>(roms.size()) + 1;
}

static bool IsSettingsSelected(const std::vector<RomEntry>& roms, int selected)
{
    return selected == static_cast<int>(roms.size());
}

static void MoveSelection(const std::vector<RomEntry>& roms,
    int& selected, int direction)
{
    int itemCount = GetMenuItemCount(roms);

    if (itemCount <= 0)
        return;

    selected += direction;

    if (selected < 0)
        selected = itemCount - 1;

    if (selected >= itemCount)
        selected = 0;
}

static void DrawFrontend(SDL_Window* window,
                         SDL_Renderer* renderer,
                         const std::vector<RomEntry>& roms,
                         int selected)
{
    if (!renderer)
        return;

    int windowW = 0;
    int windowH = 0;
    SDL_GetWindowSize(window, &windowW, &windowH);

    SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
    SDL_RenderClear(renderer);

    // Header
    SDL_SetRenderDrawColor(renderer, 28, 28, 36, 255);
    SDL_FRect header{
        0.0f, 0.0f,
        static_cast<float>(windowW),
        110.0f
    };
    SDL_RenderFillRect(renderer, &header);

    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    DrawText(renderer, 40.0f, 25.0f, 3.0f, "O2EM-NG");
    DrawText(renderer, 40.0f, 70.0f, 1.5f,
             "PHILIPS VIDEOPAC G7000 / MAGNAVOX ODYSSEY2 EMULATOR");

    // ROM list panel
    const float panelX = 60.0f;
    const float panelY = 145.0f;
    const float panelW = static_cast<float>(windowW) - 120.0f;
    const float panelH = static_cast<float>(windowH) - 255.0f;

    SDL_SetRenderDrawColor(renderer, 22, 22, 28, 255);
    SDL_FRect panel{ panelX, panelY, panelW, panelH };
    SDL_RenderFillRect(renderer, &panel);

    if (roms.empty())
    {
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        DrawText(renderer,
                 panelX + 30.0f,
                 panelY + 30.0f,
                 2.0f,
                 "NO ROM FILES FOUND IN THE ROMS FOLDER");
    }
    else
    {
        int firstVisible = selected - (VISIBLE_ROWS / 2);

        if (firstVisible < 0)
            firstVisible = 0;

        int itemCount = GetMenuItemCount(roms);

        int maxFirst =
            std::max(0, itemCount - VISIBLE_ROWS);

        if (firstVisible > maxFirst)
            firstVisible = maxFirst;

        int lastVisible =
            std::min(firstVisible + VISIBLE_ROWS, itemCount);

        float y = panelY + 22.0f;

        for (int i = firstVisible; i < lastVisible; ++i)
        {
            if (i == selected)
            {
                SDL_SetRenderDrawColor(renderer, 185, 35, 35, 255);

                SDL_FRect highlight{
                    panelX + 15.0f,
                    y - 7.0f,
                    panelW - 30.0f,
                    30.0f
                };

                SDL_RenderFillRect(renderer, &highlight);
            }

            SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);

            std::string line;

            if (IsSettingsSelected(roms, i))
            {
                line = "[ SETTINGS ]";
            }
            else
            {
                line = std::to_string(i + 1) + ". " + roms[i].name;
            }

            DrawText(renderer,
                     panelX + 30.0f,
                     y,
                     1.5f,
                     line);

            y += 32.0f;
        }
    }

    // Footer
    SDL_SetRenderDrawColor(renderer, 225, 225, 225, 255);

    DrawText(renderer,
             60.0f,
             static_cast<float>(windowH) - 75.0f,
             1.5f,
             std::to_string(roms.size()) + " GAMES");

    DrawText(renderer,
             250.0f,
             static_cast<float>(windowH) - 75.0f,
             1.5f,
             "UP/DOWN - SELECT");

    DrawText(renderer,
             535.0f,
             static_cast<float>(windowH) - 75.0f,
             1.5f,
             "ENTER/A - PLAY");

    DrawText(renderer,
             790.0f,
             static_cast<float>(windowH) - 75.0f,
             1.5f,
             "ESC/B - EXIT");

    SDL_RenderPresent(renderer);
}

static void DrawSettingsScreen(SDL_Window* window,
    SDL_Renderer* renderer,
    const O2EMSettings& settings,
    int settingsSelected)
{
    if (!renderer)
        return;

    int windowW = 0;
    int windowH = 0;
    SDL_GetWindowSize(window, &windowW, &windowH);

    SDL_SetRenderDrawColor(renderer, 10, 10, 14, 255);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 28, 28, 36, 255);
    SDL_FRect header{
        0.0f, 0.0f,
        static_cast<float>(windowW),
        110.0f
    };
    SDL_RenderFillRect(renderer, &header);

    SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
    DrawText(renderer, 40.0f, 25.0f, 3.0f, "O2EM-NG SETTINGS");
    DrawText(renderer, 40.0f, 70.0f, 1.5f,
        "CONFIGURATION / BETA OPTIONS");

    const float panelX = 120.0f;
    const float panelY = 170.0f;
    const float panelW = static_cast<float>(windowW) - 240.0f;
    const float panelH = static_cast<float>(windowH) - 300.0f;

    SDL_SetRenderDrawColor(renderer, 22, 22, 28, 255);
    SDL_FRect panel{ panelX, panelY, panelW, panelH };
    SDL_RenderFillRect(renderer, &panel);

    const float rowY[3] = {
        panelY + 50.0f,
        panelY + 110.0f,
        panelY + 170.0f
    };

    for (int i = 0; i < 3; ++i)
    {
        if (i == settingsSelected)
        {
            SDL_SetRenderDrawColor(renderer, 185, 35, 35, 255);

            SDL_FRect highlight{
                panelX + 20.0f,
                rowY[i] - 12.0f,
                panelW - 40.0f,
                42.0f
            };

            SDL_RenderFillRect(renderer, &highlight);
        }

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);

        std::string line;

        if (i == 0)
        {
            line = std::string("Start Fullscreen: ") +
                (settings.start_fullscreen ? "ON" : "OFF");
        }
        else if (i == 1)
        {
            line = std::string("Region Mode: ") +
                RegionModeToString(settings.region_mode);
        }
        else
        {
            line = std::string("Scanlines: ") +
                (settings.scanlines ? "ON" : "OFF");
        }

        DrawText(renderer,
            panelX + 40.0f,
            rowY[i],
            2.0f,
            line);
    }

    SDL_SetRenderDrawColor(renderer, 225, 225, 225, 255);

    DrawText(renderer,
        60.0f,
        static_cast<float>(windowH) - 75.0f,
        1.5f,
        "ESC/B - BACK");

    SDL_RenderPresent(renderer);
}

static SDL_Renderer* GetFrontendRenderer(SDL_Window* window)
{
    // The emulator's display cleanup may destroy the renderer.
    // Calling VDCStub_SetWindow recreates it when necessary.
    VDCStub_SetWindow(window);
    return SDL_GetRenderer(window);
}

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "SDL Error",
            SDL_GetError(),
            nullptr);

        return 1;
    }
    O2EMSettings settings = LoadSettings("o2em-ng.cfg");

    SDL_Window* window = SDL_CreateWindow(
        "O2EM-NG",
        FRONTEND_WIDTH,
        FRONTEND_HEIGHT,
        SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Window Error",
            SDL_GetError(),
            nullptr);

        SDL_Quit();
        return 1;
    }

    SDL_SetWindowMinimumSize(window, 800, 600);

    printf("O2EM-NG: forcing fullscreen test at startup...\n");
    if (settings.start_fullscreen)
    {
        if (!SDL_SetWindowFullscreen(window, true))
        {
            printf("O2EM-NG: SDL_SetWindowFullscreen failed: %s\n", SDL_GetError());
            SDL_MaximizeWindow(window);
            printf("O2EM-NG: fallback to maximized window.\n");
        }
        else
        {
            printf("O2EM-NG: fullscreen startup enabled from settings.\n");
        }
    }
    else
    {
        printf("O2EM-NG: windowed startup enabled from settings.\n");
    }

    SDL_Renderer* renderer = GetFrontendRenderer(window);

    if (!renderer)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Renderer Error",
            SDL_GetError(),
            window);

        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    std::string baseFolder = SDL_GetBasePath();
    std::string romFolder = baseFolder + "ROMS";

    std::vector<RomEntry> roms = LoadRoms(romFolder);

    int selected = 0;

    bool running = true;
    bool redraw = true;
    bool settingsScreen = false;
    int settingsSelected = 0;

    bool stickUpHeld = false;
    bool stickDownHeld = false;

    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            InputManager_HandleEvent(e);

            if (e.type == SDL_EVENT_QUIT)
            {
                running = false;
            }

            if (e.type == SDL_EVENT_WINDOW_RESIZED)
            {
                redraw = true;
            }

            if (e.type == SDL_EVENT_KEY_DOWN && !e.key.repeat)
            {
                if (e.key.key == SDLK_ESCAPE)
                {
                    if (settingsScreen)
                    {
                        settingsScreen = false;
                        redraw = true;
                        printf("O2EM-NG: Returned to ROM browser from Settings.\n");
                    }
                    else
                    {
                        running = false;
                    }
                }
                else if (e.key.key == SDLK_UP)
                {
                    if (settingsScreen)
                    {
                        settingsSelected--;

                        if (settingsSelected < 0)
                            settingsSelected = 2;
                    }
                    else
                    {
                        MoveSelection(roms, selected, -1);
                    }

                    redraw = true;
                }
                else if (e.key.key == SDLK_DOWN)
                {
                    if (settingsScreen)
                    {
                        settingsSelected++;

                        if (settingsSelected > 2)
                            settingsSelected = 0;
                    }
                    else
                    {
                        MoveSelection(roms, selected, 1);
                    }

                    redraw = true;
                }
                else if (e.key.key == SDLK_RETURN)
                {
                    if (settingsScreen)
                    {
                        if (settingsSelected == 0)
                        {
                            settings.start_fullscreen = !settings.start_fullscreen;
                        }
                        else if (settingsSelected == 1)
                        {
                            if (settings.region_mode == RegionMode::Auto)
                                settings.region_mode = RegionMode::PAL;
                            else if (settings.region_mode == RegionMode::PAL)
                                settings.region_mode = RegionMode::NTSC;
                            else
                                settings.region_mode = RegionMode::Auto;
                        }
                        else if (settingsSelected == 2)
                        {
                            settings.scanlines = !settings.scanlines;
                        }

                        SaveSettings("o2em-ng.cfg", settings);
                        printf("O2EM-NG: Settings saved.\n");
                        redraw = true;
                    }
                    else if (IsSettingsSelected(roms, selected))
                    {
                        printf("O2EM-NG: Settings screen opened.\n");
                        settingsScreen = true;
                        redraw = true;
                    }
                    else if (!roms.empty())
                    {
                        LaunchRom(window, roms[selected], settings.region_mode);

                        renderer = GetFrontendRenderer(window);
                        redraw = true;
                    }
                }
                }

            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            {
                if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_UP)
                {
                    if (settingsScreen)
                    {
                        settingsSelected--;

                        if (settingsSelected < 0)
                            settingsSelected = 2;
                    }
                    else
                    {
                        MoveSelection(roms, selected, -1);
                    }

                    redraw = true;
                }
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_DPAD_DOWN)
                {
                    if (settingsScreen)
                    {
                        settingsSelected++;

                        if (settingsSelected > 2)
                            settingsSelected = 0;
                    }
                    else
                    {
                        MoveSelection(roms, selected, 1);
                    }

                    redraw = true;
                }
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_SOUTH)
                {
                    if (settingsScreen)
                    {
                        if (settingsSelected == 0)
                        {
                            settings.start_fullscreen = !settings.start_fullscreen;
                        }
                        else if (settingsSelected == 1)
                        {
                            if (settings.region_mode == RegionMode::Auto)
                                settings.region_mode = RegionMode::PAL;
                            else if (settings.region_mode == RegionMode::PAL)
                                settings.region_mode = RegionMode::NTSC;
                            else
                                settings.region_mode = RegionMode::Auto;
                        }
                        else if (settingsSelected == 2)
                        {
                            settings.scanlines = !settings.scanlines;
                        }

                        SaveSettings("o2em-ng.cfg", settings);
                        printf("O2EM-NG: Settings saved.\n");
                        redraw = true;
                    }
                    else if (IsSettingsSelected(roms, selected))
                    {
                        printf("O2EM-NG: Settings screen opened.\n");
                        settingsScreen = true;
                        redraw = true;
                    }
                    else if (!roms.empty())
                    {
                        LaunchRom(window, roms[selected], settings.region_mode);

                        renderer = GetFrontendRenderer(window);
                        redraw = true;
                    }
                }
                else if (e.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)
                {
                    if (settingsScreen)
                    {
                        settingsScreen = false;
                        redraw = true;
                        printf("O2EM-NG: Returned to ROM browser from Settings.\n");
                    }
                    else
                    {
                        running = false;
                    }
                }
            }

            if (e.type == SDL_EVENT_GAMEPAD_AXIS_MOTION &&
                e.gaxis.axis == SDL_GAMEPAD_AXIS_LEFTY)
            {
                if (e.gaxis.value < -STICK_DEADZONE)
                {
                    if (!stickUpHeld)
                    {
                        if (settingsScreen)
                        {
                            settingsSelected--;

                            if (settingsSelected < 0)
                                settingsSelected = 2;
                        }
                        else
                        {
                            MoveSelection(roms, selected, -1);
                        }

                        redraw = true;
                    }

                    stickUpHeld = true;
                    stickDownHeld = false;
                }
                else if (e.gaxis.value > STICK_DEADZONE)
                {
                    if (!stickDownHeld)
                    {
                        if (settingsScreen)
                        {
                            settingsSelected++;

                            if (settingsSelected > 2)
                                settingsSelected = 0;
                        }
                        else
                        {
                            MoveSelection(roms, selected, 1);
                        }

                        redraw = true;
                    }

                    stickDownHeld = true;
                    stickUpHeld = false;
                }
                else
                {
                    stickUpHeld = false;
                    stickDownHeld = false;
                }
            }
        }

        if (redraw)
        {
            if (settingsScreen)
            {
                DrawSettingsScreen(window, renderer, settings, settingsSelected);
            }
            else
            {
                DrawFrontend(window, renderer, roms, selected);
            }

            redraw = false;
        }

        SDL_Delay(16);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
