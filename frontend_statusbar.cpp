#include "frontend_statusbar.h"

#include "theme_win95.h"
#include "ui_font.h"

#include <cstdio>

namespace
{
    constexpr float BarHeight = 34.0f;
    constexpr float Margin = 6.0f;
    constexpr float Gap = 6.0f;
    constexpr float CountWidth = 150.0f;
    constexpr float ExitWidth = 86.0f;

    SDL_FRect ExitButtonRect(int windowWidth, int windowHeight)
    {
        const float barY =
            static_cast<float>(windowHeight) - BarHeight - Margin;

        return {
            static_cast<float>(windowWidth) - Margin - ExitWidth,
            barY,
            ExitWidth,
            BarHeight
        };
    }
}

static void DrawStatusText(
    SDL_Renderer* renderer,
    float x,
    float y,
    const char* text)
{
    UiFont_DrawText(renderer, x, y - 2.0f, 11.0f, text ? text : "");
}

static void DrawSunkenField(
    SDL_Renderer* renderer,
    const SDL_FRect& rect)
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

static void DrawRaisedButton(
    SDL_Renderer* renderer,
    const SDL_FRect& rect)
{
    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    SDL_RenderFillRect(renderer, &rect);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);

    Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
    SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
        rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
        rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
}

bool FrontendStatusBar_HitTestExit(
    int windowWidth,
    int windowHeight,
    float x,
    float y) noexcept
{
    if (windowWidth <= 0 || windowHeight <= 0)
        return false;

    const SDL_FRect rect = ExitButtonRect(windowWidth, windowHeight);
    return x >= rect.x && x < rect.x + rect.w &&
        y >= rect.y && y < rect.y + rect.h;
}

void FrontendStatusBar_Draw(
    SDL_Renderer* renderer,
    int windowWidth,
    int windowHeight,
    int gameCount,
    const char* statusText)
{
    if (!renderer || windowWidth <= 0 || windowHeight <= 0)
        return;

    const float barY =
        static_cast<float>(windowHeight) - BarHeight - Margin;

    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    SDL_FRect background{
        0.0f,
        barY - Margin,
        static_cast<float>(windowWidth),
        BarHeight + Margin * 2.0f
    };
    SDL_RenderFillRect(renderer, &background);

    const SDL_FRect exitButton = ExitButtonRect(windowWidth, windowHeight);

    SDL_FRect countField{
        exitButton.x - Gap - CountWidth,
        barY,
        CountWidth,
        BarHeight
    };

    SDL_FRect statusField{
        Margin,
        barY,
        countField.x - Gap - Margin,
        BarHeight
    };

    DrawSunkenField(renderer, statusField);
    DrawSunkenField(renderer, countField);
    DrawRaisedButton(renderer, exitButton);

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);

    DrawStatusText(
        renderer,
        statusField.x + 8.0f,
        statusField.y + 11.0f,
        statusText ? statusText : "");

    char countText[64];
    std::snprintf(countText, sizeof(countText), "%d Games", gameCount);

    DrawStatusText(
        renderer,
        countField.x + 10.0f,
        countField.y + 11.0f,
        countText);

    DrawStatusText(
        renderer,
        exitButton.x + 24.0f,
        exitButton.y + 11.0f,
        "EXIT");
}
