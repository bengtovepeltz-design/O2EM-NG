#include "ui_font.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <unordered_map>

namespace
{
    bool gTtfInitialized = false;
    std::unordered_map<int, TTF_Font*> gFonts;

    const char* FindSystemFont()
    {
        constexpr std::array<const char*, 4> candidates = {
            "C:\\Windows\\Fonts\\tahoma.ttf",
            "C:\\Windows\\Fonts\\segoeui.ttf",
            "C:\\Windows\\Fonts\\micross.ttf",
            "C:\\Windows\\Fonts\\arial.ttf"
        };

        for (const char* path : candidates)
        {
            if (SDL_GetPathInfo(path, nullptr))
                return path;
        }
        return nullptr;
    }

    TTF_Font* GetFont(float pointSize)
    {
        if (!gTtfInitialized)
        {
            if (!TTF_Init())
                return nullptr;
            gTtfInitialized = true;
        }

        const int key = static_cast<int>(pointSize * 10.0f + 0.5f);
        const auto existing = gFonts.find(key);
        if (existing != gFonts.end())
            return existing->second;

        const char* path = FindSystemFont();
        if (!path)
            return nullptr;

        TTF_Font* font = TTF_OpenFont(path, pointSize);
        if (!font)
            return nullptr;

        gFonts.emplace(key, font);
        return font;
    }
}

bool UiFont_DrawText(
    SDL_Renderer* renderer,
    float x,
    float y,
    float pointSize,
    const std::string& text)
{
    if (!renderer || text.empty() || pointSize <= 0.0f)
        return false;

    TTF_Font* font = GetFont(pointSize);
    if (!font)
    {
        SDL_RenderDebugText(renderer, x, y, text.c_str());
        return false;
    }

    Uint8 r = 0, g = 0, b = 0, a = 255;
    SDL_GetRenderDrawColor(renderer, &r, &g, &b, &a);
    const SDL_Color color{r, g, b, a};

    SDL_Surface* surface =
        TTF_RenderText_Blended(font, text.c_str(), text.size(), color);
    if (!surface)
        return false;

    SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
    const float width = static_cast<float>(surface->w);
    const float height = static_cast<float>(surface->h);
    SDL_DestroySurface(surface);

    if (!texture)
        return false;

    const SDL_FRect destination{x, y, width, height};
    const bool rendered = SDL_RenderTexture(renderer, texture, nullptr, &destination);
    SDL_DestroyTexture(texture);
    return rendered;
}

void UiFont_Shutdown()
{
    for (auto& [size, font] : gFonts)
    {
        (void)size;
        TTF_CloseFont(font);
    }
    gFonts.clear();

    if (gTtfInitialized)
    {
        TTF_Quit();
        gTtfInitialized = false;
    }
}
