#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>
#include <cstdio>
#include <string>
#include "src/mcs48/legacy_observation.h"
#include "src/mcs48/vbl_timing_probe.h"
#include "src/mcs48/g7400_freeze_trace.h"
#include "src/version.h"

#include "src/frontend/frontend_app.h"
#include "src/frontend/startup_splash.h"
#include "vdc_stub.h"

namespace
{
    constexpr int FrontendWidth = 1600;
    constexpr int FrontendHeight = 960;

    // Real initialization stages reported by FrontendApp::Initialize (plus the
    // initial "Initializing frontend..." stage issued from here).
    constexpr int StartupStageCount = 8;
}

int main(int argc, char* argv[])
{
    int fixtureResult = 0;
    if (mcs48::observation::RunFixtureIfRequested(fixtureResult)) return fixtureResult;
    int vblProbeResult = 0;
    if (mcs48::RunVblTimingProbeIfRequested(vblProbeResult)) return vblProbeResult;
    int videoProbeResult = 0;
    if (mcs48::RunVideoProbeIfRequested(videoProbeResult)) return videoProbeResult;
    int freezeSelfTestResult = 0;
    if (mcs48::RunFreezeTraceSelfTestIfRequested(freezeSelfTestResult)) return freezeSelfTestResult;
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
    //
    // The window starts hidden so the startup splash can paint its first frame
    // before the window is ever visible: no empty or half-built window flash.
    const std::string windowTitle =
        std::string("O2EM-NG ") + O2emVersion::kReleaseName + " - " +
        O2emVersion::kAppVersion;
    SDL_Window* window = SDL_CreateWindow(
        windowTitle.c_str(),
        initialWidth,
        initialHeight,
        SDL_WINDOW_RESIZABLE | SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);

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
    const std::string baseFolder = basePath ? basePath : "";

    if (basePath) {
        const std::string iconPath = std::string(basePath) + "assets/O2EM-NG_Icon.png";
        if (SDL_Surface* icon = IMG_Load(iconPath.c_str())) {
            SDL_SetWindowIcon(window, icon);
            SDL_DestroySurface(icon);
        }
    }
    SDL_SetWindowMinimumSize(window, SDL_min(initialWidth, 1200), SDL_min(initialHeight, 900));

    // Create the shared renderer up front so the startup screen can draw into
    // the same window/renderer the frontend will use (no second window).
    VDCStub_SetWindow(window);

    int exitCode = 0;

    // Display version only: the release milestone ("Beta 5") is shown
    // separately on the splash. The project version metadata is unchanged.
    char versionDisplay[32];
    std::snprintf(versionDisplay, sizeof(versionDisplay), "v%d.%d.%d",
        O2emVersion::kAppVersionMajor,
        O2emVersion::kAppVersionMinor,
        O2emVersion::kAppVersionPatch);

    std::printf("O2EM-NG: %s %s\n", O2emVersion::kReleaseName,
                O2emVersion::kAppVersion);
    std::fflush(stdout);

    // The splash lives in its own scope so its one-shot audio stream is
    // released before SDL_Quit(), never after.
    {
        StartupSplash splash(window, "O2EM-NG", versionDisplay);
        splash.SetLogoPath(baseFolder + "assets/O2EM-NG_Icon.png");
        splash.SetStageCount(StartupStageCount);
        splash.Stage("Initializing frontend...", 0);
        splash.Show();

        FrontendApp app(window);

        if (!app.Initialize(&splash))
        {
            splash.Abort();
            SDL_ShowWindow(window);

            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_ERROR,
                "Frontend Error",
                SDL_GetError(),
                window);

            exitCode = 1;
        }
        else
        {
            // The application is fully initialized and ready: paint the final
            // state and play the real G7000 reset recording exactly once.
            splash.FinishAndPlayStartupSound(
                baseFolder + "assets/audio/reset_sound_g_7000.wav");

            SDL_Event event;

            while (app.IsRunning() && !splash.QuitRequested())
            {
                while (SDL_PollEvent(&event))
                {
                    if (!app.HandleEvent(event))
                        break;
                }

                app.Draw();
                // Release the one-shot startup audio stream once it has played.
                splash.Tick();
                SDL_Delay(16);
            }
        }
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return exitCode;
}
