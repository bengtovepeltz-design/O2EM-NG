#include "src/frontend/frontend_screenshot.h"
#include "theme_win95.h"
#include "src/library/game_info.h"

#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstddef>
#include <string>

namespace
{
    SDL_Texture* gTexture = nullptr;
    SDL_Renderer* gRenderer = nullptr;
    std::string gScreenshotKey;
    std::string gGameKey;
    std::size_t gScreenshotIndex = 0;

    void DrawText(SDL_Renderer* renderer, float x, float y, float scale,
        const std::string& text)
    {
        SDL_SetRenderScale(renderer, scale, scale);
        SDL_RenderDebugText(renderer, x / scale, y / scale, text.c_str());
        SDL_SetRenderScale(renderer, 1.0f, 1.0f);
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

    void DrawRaisedButton(SDL_Renderer* renderer, const SDL_FRect& rect,
        const char* label)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, rect.x + 13.0f, rect.y + 9.0f, 0.95f, label);
    }

    void DestroyTexture()
    {
        if (gTexture)
        {
            SDL_DestroyTexture(gTexture);
            gTexture = nullptr;
        }
    }

    std::string GameKey(const GameInfo* game)
    {
        if (!game) return {};
        return std::to_string(game->videopacNumber) + "|" + game->title;
    }

    void SynchronizeGame(const GameInfo* game)
    {
        const std::string key = GameKey(game);
        if (key != gGameKey)
        {
            gGameKey = key;
            gScreenshotIndex = 0;
            gScreenshotKey.clear();
            DestroyTexture();
        }
        if (!game || game->screenshots.empty())
            gScreenshotIndex = 0;
        else if (gScreenshotIndex >= game->screenshots.size())
            gScreenshotIndex = game->screenshots.size() - 1;
    }

    std::string BuildKey(const GameInfo* game)
    {
        if (!game || game->screenshots.empty()) return {};
        return game->screenshots[gScreenshotIndex].string() + "|" +
            std::to_string(game->screenshots.size());
    }

    void EnsureTexture(SDL_Renderer* renderer, const GameInfo* game)
    {
        SynchronizeGame(game);
        if (renderer != gRenderer)
        {
            DestroyTexture();
            gRenderer = renderer;
            gScreenshotKey.clear();
        }
        const std::string newKey = BuildKey(game);
        if (newKey == gScreenshotKey) return;
        DestroyTexture();
        gScreenshotKey = newKey;
        if (!game || game->screenshots.empty()) return;
        gTexture = IMG_LoadTexture(renderer,
            game->screenshots[gScreenshotIndex].string().c_str());
    }

    void ButtonRects(const SDL_FRect& rightContent, SDL_FRect& previous,
        SDL_FRect& next)
    {
        const float margin = 14.0f;
        const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
            rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
        const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
            frame.w - 8.0f, frame.h - 8.0f};
        previous = {inner.x + 20.0f, inner.y + inner.h - 45.0f, 116.0f, 30.0f};
        next = {inner.x + inner.w - 136.0f, inner.y + inner.h - 45.0f,
            116.0f, 30.0f};
    }

    SDL_FRect DeleteButtonRect(const SDL_FRect& rightContent)
    {
        const float margin = 14.0f;
        const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
            rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
        const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
            frame.w - 8.0f, frame.h - 8.0f};
        return {inner.x + (inner.w - 150.0f) * 0.5f,
            inner.y + inner.h - 45.0f, 150.0f, 30.0f};
    }

    bool Contains(const SDL_FRect& rect, float x, float y)
    {
        return x >= rect.x && x < rect.x + rect.w &&
            y >= rect.y && y < rect.y + rect.h;
    }
}

void FrontendScreenshot_Draw(SDL_Renderer* renderer,
    const SDL_FRect& rightContent, const GameInfo* game)
{
    if (!renderer) return;
    EnsureTexture(renderer, game);

    const float margin = 14.0f;
    const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
        rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
    DrawSunkenFrame(renderer, frame);
    const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
        frame.w - 8.0f, frame.h - 8.0f};
    Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
    SDL_RenderFillRect(renderer, &inner);
    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);

    DrawText(renderer, inner.x + 20.0f, inner.y + 18.0f, 1.55f, "SCREENSHOT");
    if (!game)
    {
        DrawText(renderer, inner.x + 20.0f, inner.y + 58.0f, 1.0f, "SELECT A GAME");
        return;
    }
    DrawText(renderer, inner.x + 20.0f, inner.y + 58.0f, 1.0f,
        game->title);
    if (game->screenshots.empty())
    {
        DrawText(renderer, inner.x + 20.0f, inner.y + 98.0f, 1.0f,
            "NO SCREENSHOTS AVAILABLE");
        DrawText(renderer, inner.x + 20.0f, inner.y + 124.0f, 0.9f,
            "Use Import Center > Add Screenshot to import one.");
        return;
    }

    const float footerHeight = 62.0f;
    const SDL_FRect imageArea{inner.x + 20.0f, inner.y + 88.0f,
        inner.w - 40.0f, inner.h - 88.0f - footerHeight};
    Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
    SDL_RenderFillRect(renderer, &imageArea);

    if (gTexture)
    {
        float imageWidth = 0.0f, imageHeight = 0.0f;
        if (SDL_GetTextureSize(gTexture, &imageWidth, &imageHeight) &&
            imageWidth > 0.0f && imageHeight > 0.0f)
        {
            const float scale = (std::min)(imageArea.w / imageWidth,
                imageArea.h / imageHeight);
            const SDL_FRect destination{
                imageArea.x + (imageArea.w - imageWidth * scale) * 0.5f,
                imageArea.y + (imageArea.h - imageHeight * scale) * 0.5f,
                imageWidth * scale, imageHeight * scale};
            SDL_RenderTexture(renderer, gTexture, nullptr, &destination);
        }
    }
    else
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        DrawText(renderer, imageArea.x + 18.0f,
            imageArea.y + imageArea.h * 0.5f, 1.0f,
            "SCREENSHOT COULD NOT BE LOADED");
    }

    SDL_FRect previous{}, next{};
    ButtonRects(rightContent, previous, next);
    if (game->screenshots.size() > 1)
    {
        DrawRaisedButton(renderer, previous, "< PREVIOUS");
        DrawRaisedButton(renderer, next, "NEXT >");
    }
    DrawRaisedButton(renderer, DeleteButtonRect(rightContent), "DELETE");

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    const std::string filename =
        game->screenshots[gScreenshotIndex].filename().string();
    const std::string counter = "Screenshot " +
        std::to_string(gScreenshotIndex + 1) + " of " +
        std::to_string(game->screenshots.size()) + "  -  " + filename;
    DrawText(renderer, inner.x + 154.0f, inner.y + inner.h - 61.0f,
        0.9f, counter);
}

void FrontendScreenshot_Move(const GameInfo* game, int direction)
{
    SynchronizeGame(game);
    if (!game || game->screenshots.size() < 2 || direction == 0) return;
    const std::size_t count = game->screenshots.size();
    if (direction < 0)
        gScreenshotIndex = (gScreenshotIndex + count - 1) % count;
    else
        gScreenshotIndex = (gScreenshotIndex + 1) % count;
    gScreenshotKey.clear();
}

bool FrontendScreenshot_HitTest(const SDL_FRect& rightContent,
    const GameInfo* game, float x, float y)
{
    if (!game || game->screenshots.size() < 2) return false;
    SDL_FRect previous{}, next{};
    ButtonRects(rightContent, previous, next);
    if (Contains(previous, x, y))
    {
        FrontendScreenshot_Move(game, -1);
        return true;
    }
    if (Contains(next, x, y))
    {
        FrontendScreenshot_Move(game, 1);
        return true;
    }
    return false;
}

bool FrontendScreenshot_DeleteHitTest(const SDL_FRect& rightContent,
    const GameInfo* game, float x, float y)
{
    if (!game || game->screenshots.empty()) return false;
    return Contains(DeleteButtonRect(rightContent), x, y);
}

std::filesystem::path FrontendScreenshot_CurrentPath(const GameInfo* game)
{
    SynchronizeGame(game);
    if (!game || game->screenshots.empty() ||
        gScreenshotIndex >= game->screenshots.size())
    {
        return {};
    }
    return game->screenshots[gScreenshotIndex];
}

void FrontendScreenshot_Invalidate()
{
    DestroyTexture();
    gScreenshotKey.clear();
}

void FrontendScreenshot_Shutdown()
{
    DestroyTexture();
    gRenderer = nullptr;
    gScreenshotKey.clear();
    gGameKey.clear();
    gScreenshotIndex = 0;
}
