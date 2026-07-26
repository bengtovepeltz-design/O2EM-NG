#pragma once

#include <SDL3/SDL.h>

namespace Win95Theme
{
    extern const SDL_Color Face;
    extern const SDL_Color Highlight;
    extern const SDL_Color Light;
    extern const SDL_Color Shadow;
    extern const SDL_Color DarkShadow;

    extern const SDL_Color Window;
    extern const SDL_Color WindowText;

    extern const SDL_Color ActiveTitle;
    extern const SDL_Color ActiveTitleText;

    extern const SDL_Color SelectedItem;
    extern const SDL_Color SelectedItemText;

    extern const SDL_Color TabInactive;
    extern const SDL_Color TabInactiveText;
    extern const SDL_Color TabActive;
    extern const SDL_Color TabActiveText;

    void SetRenderColor(SDL_Renderer* renderer, const SDL_Color& color);
}
