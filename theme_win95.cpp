#include "theme_win95.h"

namespace Win95Theme
{
    const SDL_Color Face{ 192, 192, 192, 255 };
    const SDL_Color Highlight{ 255, 255, 255, 255 };
    const SDL_Color Light{ 223, 223, 223, 255 };
    const SDL_Color Shadow{ 128, 128, 128, 255 };
    const SDL_Color DarkShadow{ 64, 64, 64, 255 };

    const SDL_Color Window{ 255, 255, 255, 255 };
    const SDL_Color WindowText{ 0, 0, 0, 255 };

    const SDL_Color ActiveTitle{ 0, 0, 128, 255 };
    const SDL_Color ActiveTitleText{ 255, 255, 255, 255 };

    const SDL_Color SelectedItem{ 185, 35, 35, 255 };
    const SDL_Color SelectedItemText{ 255, 255, 255, 255 };

    const SDL_Color TabInactive{ 192, 192, 192, 255 };
    const SDL_Color TabInactiveText{ 0, 0, 0, 255 };
    const SDL_Color TabActive{ 215, 165, 55, 255 };
    const SDL_Color TabActiveText{ 0, 0, 0, 255 };

    void SetRenderColor(SDL_Renderer* renderer, const SDL_Color& color)
    {
        if (!renderer)
            return;

        SDL_SetRenderDrawColor(
            renderer,
            color.r,
            color.g,
            color.b,
            color.a);
    }
}
