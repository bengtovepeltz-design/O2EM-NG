#include <SDL3/SDL.h>

#include "src/frontend/frontend_app.h"

namespace
{
    constexpr int FrontendWidth = 1200;
    constexpr int FrontendHeight = 900;
}

int main(int argc, char* argv[])
{
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

    SDL_Window* window = SDL_CreateWindow(
        "O2EM-NG",
        FrontendWidth,
        FrontendHeight,
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
