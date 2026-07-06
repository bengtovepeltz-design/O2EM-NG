#include <SDL3/SDL.h>
#include <stdio.h>

int main(int argc, char* argv[])
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "SDL Init Error",
            SDL_GetError(),
            nullptr
        );
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "O2EM-NG SDL3 Controller Test",
        800,
        600,
        SDL_WINDOW_RESIZABLE
    );

    if (!window)
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR,
            "Window Error",
            SDL_GetError(),
            nullptr
        );
        SDL_Quit();
        return 1;
    }

    SDL_Gamepad* gamepad = nullptr;

    int count = 0;
    SDL_JoystickID* gamepads = SDL_GetGamepads(&count);

    if (count > 0)
    {
        gamepad = SDL_OpenGamepad(gamepads[0]);

        if (gamepad)
        {
            const char* name = SDL_GetGamepadName(gamepad);

            SDL_ShowSimpleMessageBox(
                SDL_MESSAGEBOX_INFORMATION,
                "Controller Detected",
                name ? name : "Unknown controller",
                window
            );
        }
    }
    else
    {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_WARNING,
            "No Controller",
            "No SDL gamepad detected.",
            window
        );
    }

    if (gamepads)
    {
        SDL_free(gamepads);
    }

    bool running = true;
    SDL_Event e;

    while (running)
    {
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_EVENT_QUIT)
            {
                running = false;
            }

            if (e.type == SDL_EVENT_KEY_DOWN)
            {
                if (e.key.key == SDLK_ESCAPE)
                {
                    running = false;
                }
            }

            if (e.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
            {
                switch (e.gbutton.button)
                {
                case SDL_GAMEPAD_BUTTON_SOUTH:
                    SDL_ShowSimpleMessageBox(
                        SDL_MESSAGEBOX_INFORMATION,
                        "Button",
                        "A / South button pressed",
                        window
                    );
                    break;

                case SDL_GAMEPAD_BUTTON_EAST:
                    SDL_ShowSimpleMessageBox(
                        SDL_MESSAGEBOX_INFORMATION,
                        "Button",
                        "B / East button pressed",
                        window
                    );
                    break;

                case SDL_GAMEPAD_BUTTON_DPAD_UP:
                    SDL_ShowSimpleMessageBox(
                        SDL_MESSAGEBOX_INFORMATION,
                        "Button",
                        "D-Pad Up pressed",
                        window
                    );
                    break;

                case SDL_GAMEPAD_BUTTON_DPAD_DOWN:
                    SDL_ShowSimpleMessageBox(
                        SDL_MESSAGEBOX_INFORMATION,
                        "Button",
                        "D-Pad Down pressed",
                        window
                    );
                    break;
                }
            }
        }

        SDL_Delay(16);
    }

    if (gamepad)
    {
        SDL_CloseGamepad(gamepad);
    }

    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}