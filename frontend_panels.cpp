#include "frontend_panels.h"
#include "theme_win95.h"

#include <algorithm>

static void DrawRaisedFrame(SDL_Renderer* renderer, const SDL_FRect& rect)
{
    // Windows 95 face.
    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    SDL_RenderFillRect(renderer, &rect);

    // Top and left highlights.
    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Light);
    SDL_RenderLine(
        renderer,
        rect.x + 1.0f,
        rect.y + 1.0f,
        rect.x + rect.w - 2.0f,
        rect.y + 1.0f);
    SDL_RenderLine(
        renderer,
        rect.x + 1.0f,
        rect.y + 1.0f,
        rect.x + 1.0f,
        rect.y + rect.h - 2.0f);

    // Bottom and right shadows.
    Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
    SDL_RenderLine(
        renderer,
        rect.x,
        rect.y + rect.h - 1.0f,
        rect.x + rect.w - 1.0f,
        rect.y + rect.h - 1.0f);
    SDL_RenderLine(
        renderer,
        rect.x + rect.w - 1.0f,
        rect.y,
        rect.x + rect.w - 1.0f,
        rect.y + rect.h - 1.0f);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(
        renderer,
        rect.x + 1.0f,
        rect.y + rect.h - 2.0f,
        rect.x + rect.w - 2.0f,
        rect.y + rect.h - 2.0f);
    SDL_RenderLine(
        renderer,
        rect.x + rect.w - 2.0f,
        rect.y + 1.0f,
        rect.x + rect.w - 2.0f,
        rect.y + rect.h - 2.0f);
}

static void DrawSunkenContent(SDL_Renderer* renderer, const SDL_FRect& rect)
{
    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderFillRect(renderer, &rect);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
    SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);

    Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
    SDL_RenderLine(
        renderer,
        rect.x + 1.0f,
        rect.y + 1.0f,
        rect.x + rect.w - 2.0f,
        rect.y + 1.0f);
    SDL_RenderLine(
        renderer,
        rect.x + 1.0f,
        rect.y + 1.0f,
        rect.x + 1.0f,
        rect.y + rect.h - 2.0f);

    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(
        renderer,
        rect.x,
        rect.y + rect.h - 1.0f,
        rect.x + rect.w - 1.0f,
        rect.y + rect.h - 1.0f);
    SDL_RenderLine(
        renderer,
        rect.x + rect.w - 1.0f,
        rect.y,
        rect.x + rect.w - 1.0f,
        rect.y + rect.h - 1.0f);
}

FrontendPanelLayout FrontendPanels_Calculate(
    int windowWidth,
    int windowHeight)
{
    const float width = static_cast<float>(windowWidth);
    const float height = static_cast<float>(windowHeight);

    const float margin =
        std::clamp(width * 0.015f, 12.0f, 28.0f);

    const float gap =
        std::clamp(width * 0.010f, 8.0f, 18.0f);

    const float top = 196.0f;
    const float bottom = 96.0f;

    const float usableWidth = width - (margin * 2.0f) - gap;
    const float usableHeight = std::max(220.0f, height - top - bottom);

    // Keep the ROM list useful on small windows, but allow the
    // right panel to become dominant on wide fullscreen displays.
    float leftWidth = usableWidth * 0.37f;
    leftWidth = std::clamp(leftWidth, 300.0f, 620.0f);

    float rightWidth = usableWidth - leftWidth;

    if (rightWidth < 360.0f)
    {
        rightWidth = 360.0f;
        leftWidth = usableWidth - rightWidth;
    }

    FrontendPanelLayout result{};

    result.leftOuter = {
        margin,
        top,
        leftWidth,
        usableHeight
    };

    result.rightOuter = {
        margin + leftWidth + gap,
        top,
        rightWidth,
        usableHeight
    };

    constexpr float frame = 7.0f;

    result.leftContent = {
        result.leftOuter.x + frame,
        result.leftOuter.y + frame,
        result.leftOuter.w - frame * 2.0f,
        result.leftOuter.h - frame * 2.0f
    };

    result.rightContent = {
        result.rightOuter.x + frame,
        result.rightOuter.y + frame,
        result.rightOuter.w - frame * 2.0f,
        result.rightOuter.h - frame * 2.0f
    };

    return result;
}

void FrontendPanels_Draw(
    SDL_Renderer* renderer,
    const FrontendPanelLayout& layout)
{
    if (!renderer)
        return;

    DrawRaisedFrame(renderer, layout.leftOuter);
    DrawSunkenContent(renderer, layout.leftContent);

    DrawRaisedFrame(renderer, layout.rightOuter);
    DrawSunkenContent(renderer, layout.rightContent);

}
