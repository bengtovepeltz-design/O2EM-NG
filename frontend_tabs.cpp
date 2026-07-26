#include "frontend_tabs.h"
#include "theme_win95.h"
#include "ui_font.h"

#include <array>

namespace
{
    constexpr std::array<const char*, static_cast<size_t>(FrontendTab::Count)> Tabs = {
        "Library", "Game Data", "Import Center", "Screenshot",
        "Manual", "Settings", "About", "Credits"
    };

    void DrawRaisedEdge(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }

    void DrawPressedEdge(SDL_Renderer* renderer, const SDL_FRect& rect)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
        SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
        Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
        SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
            rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
    }
}

int FrontendTabs_GetCount() noexcept
{
    return static_cast<int>(FrontendTab::Count);
}

const char* FrontendTabs_GetName(FrontendTab tab) noexcept
{
    const int index = static_cast<int>(tab);
    return index >= 0 && index < FrontendTabs_GetCount()
        ? Tabs[static_cast<size_t>(index)] : "Unknown";
}

void FrontendTabs_Draw(SDL_Renderer* renderer, int windowWidth, FrontendTab activeTab)
{
    if (!renderer || windowWidth <= 0)
        return;

    const float barY = 142.0f;
    const float barH = 42.0f;
    const float marginX = 18.0f;
    const float gap = 2.0f;
    const float availableWidth = static_cast<float>(windowWidth) - marginX * 2.0f -
        gap * static_cast<float>(Tabs.size() - 1);
    const float tabWidth = availableWidth / static_cast<float>(Tabs.size());

    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    const SDL_FRect bar{0.0f, barY, static_cast<float>(windowWidth), barH};
    SDL_RenderFillRect(renderer, &bar);

    // Classic separator underneath the tab strip.
    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(renderer, 0.0f, barY, static_cast<float>(windowWidth), barY);
    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(renderer, 0.0f, barY + barH - 1.0f,
        static_cast<float>(windowWidth), barY + barH - 1.0f);

    const int activeIndex = static_cast<int>(activeTab);
    for (size_t i = 0; i < Tabs.size(); ++i)
    {
        const float x = marginX + static_cast<float>(i) * (tabWidth + gap);
        SDL_FRect tab{x, barY + 6.0f, tabWidth, 30.0f};
        const bool active = static_cast<int>(i) == activeIndex;

        Win95Theme::SetRenderColor(renderer,
            active ? Win95Theme::TabActive : Win95Theme::Face);
        SDL_RenderFillRect(renderer, &tab);
        active ? DrawPressedEdge(renderer, tab) : DrawRaisedEdge(renderer, tab);

        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        UiFont_DrawText(renderer, tab.x + 12.0f + (active ? 1.0f : 0.0f),
            tab.y + 6.0f + (active ? 1.0f : 0.0f), 12.0f, Tabs[i]);
    }
}
