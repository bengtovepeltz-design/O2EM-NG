#include "ui_font.h"

#include <SDL3_ttf/SDL_ttf.h>

#include <array>
#include <unordered_map>

namespace
{
    bool gTtfInitialized = false;
    struct UiFontEntry
    {
        TTF_Font* regular = nullptr;
        TTF_Font* bold = nullptr;
    };
    std::unordered_map<int, UiFontEntry> gFonts;

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

    const char* FindSystemBoldFont()
    {
        constexpr std::array<const char*, 4> candidates = {
            "C:\\Windows\\Fonts\\tahomabd.ttf",
            "C:\\Windows\\Fonts\\segoeuib.ttf",
            "C:\\Windows\\Fonts\\arialbd.ttf"
        };

        for (const char* path : candidates)
        {
            if (SDL_GetPathInfo(path, nullptr))
                return path;
        }
        return nullptr;
    }

    TTF_Font* GetFont(float pointSize, bool bold)
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
            return bold ? existing->second.bold : existing->second.regular;

        UiFontEntry entry;

        const char* path = FindSystemFont();
        if (path)
        {
            entry.regular = TTF_OpenFont(path, pointSize);
            if (entry.regular)
            {
                const char* boldPath = FindSystemBoldFont();
                if (boldPath)
                    entry.bold = TTF_OpenFont(boldPath, pointSize);
                if (!entry.bold && entry.regular)
                {
                    // No bold face found: reuse the regular face; callers keep
                    // their regular-weight fallback behavior.
                    entry.bold = entry.regular;
                }
            }
        }

        if (!entry.regular)
            return nullptr;

        gFonts.emplace(key, entry);
        return bold ? entry.bold : entry.regular;
    }

    bool DrawTextWithFont(
        SDL_Renderer* renderer,
        float x,
        float y,
        float pointSize,
        const std::string& text,
        bool bold)
    {
        if (!renderer || text.empty() || pointSize <= 0.0f)
            return false;

        TTF_Font* font = GetFont(pointSize, bold);
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
}

bool UiFont_DrawText(
    SDL_Renderer* renderer,
    float x,
    float y,
    float pointSize,
    const std::string& text)
{
    return DrawTextWithFont(renderer, x, y, pointSize, text, false);
}

bool UiFont_DrawTextBold(
    SDL_Renderer* renderer,
    float x,
    float y,
    float pointSize,
    const std::string& text)
{
    return DrawTextWithFont(renderer, x, y, pointSize, text, true);
}

bool UiFont_MeasureText(
    float pointSize,
    const std::string& text,
    float* outWidth,
    float* outHeight,
    bool bold)
{
    if (text.empty() || pointSize <= 0.0f)
        return false;

    TTF_Font* font = GetFont(pointSize, bold);
    if (!font)
        return false;

    int w = 0;
    int h = 0;
    if (!TTF_GetStringSize(font, text.c_str(), text.size(), &w, &h))
        return false;

    if (outWidth)
        *outWidth = static_cast<float>(w);
    if (outHeight)
        *outHeight = static_cast<float>(h);
    return true;
}

void UiFont_Shutdown()
{
    for (auto& [size, entry] : gFonts)
    {
        (void)size;
        if (entry.bold && entry.bold != entry.regular)
            TTF_CloseFont(entry.bold);
        if (entry.regular)
            TTF_CloseFont(entry.regular);
    }
    gFonts.clear();

    if (gTtfInitialized)
    {
        TTF_Quit();
        gTtfInitialized = false;
    }
}
