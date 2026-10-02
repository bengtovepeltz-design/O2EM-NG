#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <string>
#include "src/mcs48/legacy_observation.h"
#include "src/mcs48/vbl_timing_probe.h"

#include "src/frontend/frontend_app.h"

namespace
{
    constexpr int FrontendWidth = 1600;
    constexpr int FrontendHeight = 960;
}

int main(int argc, char* argv[])
{
    int fixtureResult = 0;
    if (mcs48::observation::RunFixtureIfRequested(fixtureResult)) return fixtureResult;
    int vblProbeResult = 0;
    if (mcs48::RunVblTimingProbeIfRequested(vblProbeResult)) return vblProbeResult;
    (void)argc;
    (void)argv;

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "SDL Error",
            SDL_GetError(),
            nullptr);

        return 1;
    }

    int initialWidth = FrontendWidth, initialHeight = FrontendHeight;
    SDL_Rect workArea{};
    if (SDL_GetDisplayUsableBounds(SDL_GetPrimaryDisplay(), &workArea))
    {
        initialWidth = SDL_min(initialWidth, workArea.w - 40);
        initialHeight = SDL_min(initialHeight, workArea.h - 60);
    }
    // Borderless SDL window: the frontend draws its own classic Win95 title
    // bar + menu bar (frontend_chrome.*), so the modern native frame is
    // replaced instead of duplicated. The window stays resizable and all
    // move / minimize / maximize / close / fullscreen behaviour is provided
    // by the custom chrome and the SDL window API.
    SDL_Window* window = SDL_CreateWindow(
        "O2EM-NG",
        initialWidth,
        initialHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS);

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

    const char* basePath = SDL_GetBasePath();
    if (basePath) {
        const std::string iconPath = std::string(basePath) + "assets/O2EM-NG_Icon.png";
        if (SDL_Surface* icon = IMG_Load(iconPath.c_str())) {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        }
    }
    SDL_SetWindowMinimumSize(window, SDL_min(initialWidth, 1200), SDL_min(initialHeight, 900));

    int exitCode = 0;

    {
        FrontendApp app(window);

        if (!app.Initialize())
        {
            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Frontend Error",
                SDL_GetError(),
                window);

            exitCode = 1;
        }
        else
        {
            SDL_Event event;

            while (app.IsRunning())
            {
                while (SDL_PollEvent(&event))
                {
                    if (!app.HandleEvent(event))
                        break;
                }

                app.Draw();
                SDL_Delay(16);
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return exitCode;
}
