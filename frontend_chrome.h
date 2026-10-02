#pragma once

#include <SDL3/SDL.h>

// ============================================================================
// O2EM-NG Win95 top chrome
//
// The application runs as a borderless window so the whole client area can be
// styled as a classic Windows 95 window without duplicating the modern native
// title bar. This module owns the title bar, the menu bar and the shared
// vertical layout constants the rest of the frontend derives from.
// ============================================================================
namespace FrontendChrome
{
    inline constexpr float TitleBarHeight = 26.0f;
    inline constexpr float MenuBarHeight = 22.0f;
    inline constexpr float ChromeHeight = TitleBarHeight + MenuBarHeight;

    // Framed banner (Win95 bevel + small gray margin) below the chrome.
    inline constexpr float BannerMargin = 6.0f;
    inline constexpr float BannerHeight = 110.0f;
    inline constexpr float BannerTop = ChromeHeight + BannerMargin;

    // Existing navigation row and content area, shifted down by the chrome.
    inline constexpr float TabsTop = BannerTop + BannerHeight + 6.0f;
    inline constexpr float TabsHeight = 42.0f;
    inline constexpr float ContentTop = TabsTop + TabsHeight + 12.0f;

    enum class WindowButton
    {
        None = 0,
        Minimize,
        Maximize,
        Close
    };

    // Draws the title bar + menu bar (queries the mouse for hover/pressed).
    void Draw(SDL_Window* window, SDL_Renderer* renderer, const char* title);

    // Hit testing for the custom window controls and the menu row.
    WindowButton HitTestWindowButton(int windowWidth, float x, float y);
    int MenuItemAt(int windowWidth, float x, float y);   // -1 when none
    bool IsInTitleBar(float x, float y);

    void Shutdown();
}
