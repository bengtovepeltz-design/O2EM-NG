#include "frontend_boxart.h"
#include "theme_win95.h"
#include "src/library/game_info.h"

#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <filesystem>
#include <string>

namespace
{
    SDL_Texture* gTexture = nullptr;
    SDL_Renderer* gRenderer = nullptr;
    std::string gGameKey;

    void DrawText(
        SDL_Renderer* renderer,
        float x,
        float y,
        float scale,
        const std::string& text)
    {
        SDL_SetRenderScale(renderer, scale, scale);
        SDL_RenderDebugText(renderer, x / scale, y / scale, text.c_str());
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
    }

    void DestroyTexture()
    {
        if (gTexture)
        {
            SDL_DestroyTexture(gTexture);
            gTexture = nullptr;
        }
    }

    void EnsureTexture(SDL_Renderer* renderer, const GameInfo* game)
    {
        if (renderer != gRenderer)
        {
            DestroyTexture();
            gRenderer = renderer;
            gGameKey.clear();
        }

        const std::string newKey = game ? game->romPath.string() : std::string();
        if (newKey == gGameKey)
            return;

        DestroyTexture();
        gGameKey = newKey;

        if (!game || game->boxArt.empty())
            return;

        const std::string imagePath = game->boxArt.string();
        gTexture = IMG_LoadTexture(renderer, imagePath.c_str());
    }

    void DrawSunkenFrame(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);

        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }
}


void FrontendBoxArt_DrawImage(
    SDL_Renderer* renderer,
    const SDL_FRect& imageArea,
    const GameInfo* game)
{
    if (!renderer)
        return;

    EnsureTexture(renderer, game);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
    SDL_RenderFillRect(renderer, &imageArea);

    if (gTexture)
    {
        float imageWidth = 0.0f;
        float imageHeight = 0.0f;
        if (SDL_GetTextureSize(gTexture, &imageWidth, &imageHeight) &&
            imageWidth > 0.0f && imageHeight > 0.0f)
        {
            const float scale = (std::min)(
                imageArea.w / imageWidth,
                imageArea.h / imageHeight);
            const SDL_FRect destination{
                imageArea.x + (imageArea.w - imageWidth * scale) * 0.5f,
                imageArea.y + (imageArea.h - imageHeight * scale) * 0.5f,
                imageWidth * scale,
                imageHeight * scale
            };
            SDL_RenderTexture(renderer, gTexture, nullptr, &destination);
            return;
        }
    }

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    DrawText(renderer, imageArea.x + 18.0f, imageArea.y + imageArea.h * 0.5f,
        1.0f, game ? "NO BOX ART AVAILABLE" : "SELECT A GAME");
}

void FrontendBoxArt_Draw(
    SDL_Renderer* renderer,
    const SDL_FRect& rightContent,
    const GameInfo* game)
{
    if (!renderer)
        return;

    EnsureTexture(renderer, game);

    const float margin = 14.0f;
    const SDL_FRect frame{
        rightContent.x + margin,
        rightContent.y + margin,
        rightContent.w - margin * 2.0f,
        rightContent.h - margin * 2.0f
    };
    DrawSunkenFrame(renderer, frame);

    const SDL_FRect inner{
        frame.x + 4.0f,
        frame.y + 4.0f,
        frame.w - 8.0f,
        frame.h - 8.0f
    };
    Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
    SDL_RenderFillRect(renderer, &inner);

    if (!game)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, inner.x + 30.0f, inner.y + 40.0f, 1.5f, "SELECT A GAME");
        return;
    }

    const float footerHeight = 112.0f;
    const SDL_FRect imageArea{
        inner.x + 18.0f,
        inner.y + 18.0f,
        inner.w - 36.0f,
        inner.h - footerHeight - 30.0f
    };

    if (gTexture)
    {
        float imageWidth = 0.0f;
        float imageHeight = 0.0f;
        if (SDL_GetTextureSize(gTexture, &imageWidth, &imageHeight) &&
            imageWidth > 0.0f && imageHeight > 0.0f)
        {
            const float scale = std::min(
                imageArea.w / imageWidth,
                imageArea.h / imageHeight);
            const SDL_FRect destination{
                imageArea.x + (imageArea.w - imageWidth * scale) * 0.5f,
                imageArea.y + (imageArea.h - imageHeight * scale) * 0.5f,
                imageWidth * scale,
                imageHeight * scale
            };
            SDL_RenderTexture(renderer, gTexture, nullptr, &destination);
        }
    }
    else
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(
            renderer,
            imageArea.x + 24.0f,
            imageArea.y + imageArea.h * 0.5f,
            1.25f,
            "NO BOX ART AVAILABLE");
    }

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    const float textY = inner.y + inner.h - footerHeight + 12.0f;
    DrawText(renderer, inner.x + 24.0f, textY, 1.65f, game->title);

    std::string cartridgeLine = "Official Videopac cartridge";
    if (game->videopacNumber > 0)
        cartridgeLine += " #" + std::to_string(game->videopacNumber);
    DrawText(renderer, inner.x + 24.0f, textY + 38.0f, 1.05f, cartridgeLine);

    const std::string summary = game->shortDescription.empty()
        ? "Press Enter/A or double-click to play"
        : game->shortDescription;
    DrawText(renderer, inner.x + 24.0f, textY + 66.0f, 1.0f, summary);
}

void FrontendBoxArt_Shutdown()
{
    DestroyTexture();
    gRenderer = nullptr;
    gGameKey.clear();
}
