#include "frontend_chrome.h"

#include <SDL3_image/SDL_image.h>

#include <string>

#include "theme_win95.h"
#include "ui_font.h"

namespace
{
    constexpr const char* kMenuItems[] = { "File", "Library", "Settings", "Tools", "Help" };
    constexpr int kMenuCount = static_cast<int>(sizeof(kMenuItems) / sizeof(kMenuItems[0]));
    constexpr float kMenuTextSize = 14.0f;
    constexpr float kMenuPadX = 16.0f;

    SDL_Texture* gIconTexture = nullptr;
    SDL_Renderer* gIconRenderer = nullptr;

    void DestroyIcon()
    {
        if (gIconTexture)
        {
            SDL_DestroyTexture(gIconTexture);
            gIconTexture = nullptr;
        }
        gIconRenderer = nullptr;
    }

    void EnsureIcon(SDL_Renderer* renderer)
    {
        if (renderer != gIconRenderer)
        {
            DestroyIcon();
            gIconRenderer = renderer;
        }
        if (gIconTexture)
            return;

        const char* basePath = SDL_GetBasePath();
        const std::string path =
            std::string(basePath ? basePath : "") + "assets/O2EM-NG_Icon.png";
        gIconTexture = IMG_LoadTexture(renderer, path.c_str());
    }

    float MeasureMenuLabel(const char* label)
    {
        float w = 0.0f;
        float h = 0.0f;
        if (UiFont_MeasureText(kMenuTextSize, label, &w, &h))
            return w;
        return 8.0f * static_cast<float>(std::char_traits<char>::length(label));
    }

    void LayoutMenuItems(SDL_FRect out[kMenuCount])
    {
        float x = 6.0f;
        for (int i = 0; i < kMenuCount; ++i)
        {
            const float w = MeasureMenuLabel(kMenuItems[i]) + kMenuPadX;
            out[i] = { x, FrontendChrome::TitleBarHeight + 1.0f, w,
                FrontendChrome::MenuBarHeight - 2.0f };
            x += w;
        }
    }

    void LayoutWindowButtons(int windowWidth, SDL_FRect out[3])
    {
        const float bw = 21.0f;
        const float bh = FrontendChrome::TitleBarHeight - 8.0f;
        const float gap = 2.0f;
        float x = static_cast<float>(windowWidth) - 4.0f - bw;
        for (int i = 2; i >= 0; --i)   // 0 minimize, 1 maximize, 2 close
        {
            out[i] = { x, 4.0f, bw, bh };
            x -= (bw + gap);
        }
    }

    void DrawRaisedButton(SDL_Renderer* renderer, const SDL_FRect& rect, bool pressed)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &rect);
        if (pressed)
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
        else
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x + rect.w - 1.0f, rect.y);
            SDL_RenderLine(renderer, rect.x, rect.y, rect.x, rect.y + rect.h - 1.0f);
            Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
            SDL_RenderLine(renderer, rect.x, rect.y + rect.h - 1.0f,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
            SDL_RenderLine(renderer, rect.x + rect.w - 1.0f, rect.y,
                rect.x + rect.w - 1.0f, rect.y + rect.h - 1.0f);
        }
    }

    void DrawWindowGlyph(SDL_Renderer* renderer, const SDL_FRect& rect,
        FrontendChrome::WindowButton kind)
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        const float cx = rect.x + rect.w * 0.5f;
        const float cy = rect.y + rect.h * 0.5f;
        switch (kind)
        {
        case FrontendChrome::WindowButton::Minimize:
            SDL_RenderLine(renderer, cx - 4.0f, cy + 3.0f, cx + 4.0f, cy + 3.0f);
            SDL_RenderLine(renderer, cx - 4.0f, cy + 4.0f, cx + 4.0f, cy + 4.0f);
            break;
        case FrontendChrome::WindowButton::Maximize:
        {
            const SDL_FRect box{ cx - 5.0f, cy - 4.0f, 10.0f, 9.0f };
            SDL_RenderRect(renderer, &box);
            SDL_RenderLine(renderer, box.x + 1.0f, box.y + 1.0f,
                box.x + box.w - 2.0f, box.y + 1.0f);
            break;
        }
        case FrontendChrome::WindowButton::Close:
            SDL_RenderLine(renderer, cx - 4.0f, cy - 4.0f, cx + 4.0f, cy + 4.0f);
            SDL_RenderLine(renderer, cx + 4.0f, cy - 4.0f, cx - 4.0f, cy + 4.0f);
            SDL_RenderLine(renderer, cx - 3.0f, cy - 4.0f, cx + 5.0f, cy + 4.0f);
            SDL_RenderLine(renderer, cx + 5.0f, cy - 4.0f, cx - 3.0f, cy + 4.0f);
            break;
        default:
            break;
        }
    }
}

void FrontendChrome::Draw(SDL_Window* window, SDL_Renderer* renderer,
    const char* title)
{
    if (!window || !renderer)
        return;

    int windowWidth = 0;
    int windowHeight = 0;
    SDL_GetWindowSize(window, &windowWidth, &windowHeight);

    float mouseX = 0.0f;
    float mouseY = 0.0f;
    const SDL_MouseButtonFlags buttons = SDL_GetMouseState(&mouseX, &mouseY);
    const bool leftDown = (buttons & SDL_BUTTON_LMASK) != 0;

    // ---- Title bar -------------------------------------------------------
    Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
    const SDL_FRect titleBar{ 0.0f, 0.0f, static_cast<float>(windowWidth),
        TitleBarHeight };
    SDL_RenderFillRect(renderer, &titleBar);

    EnsureIcon(renderer);
    const SDL_FRect iconRect{ 5.0f, 5.0f, 16.0f, 16.0f };
    if (gIconTexture)
    {
        SDL_RenderTexture(renderer, gIconTexture, nullptr, &iconRect);
    }
    else
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
        SDL_RenderFillRect(renderer, &iconRect);
        Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitle);
        const SDL_FRect inner{ iconRect.x + 3.0f, iconRect.y + 3.0f, 10.0f, 10.0f };
        SDL_RenderFillRect(renderer, &inner);
    }

    Win95Theme::SetRenderColor(renderer, Win95Theme::ActiveTitleText);
    UiFont_DrawText(renderer, 27.0f, 5.0f, 14.0f, title ? title : "O2EM-NG");

    SDL_FRect windowButtons[3];
    LayoutWindowButtons(windowWidth, windowButtons);
    const WindowButton buttonKinds[3] = {
        WindowButton::Minimize, WindowButton::Maximize, WindowButton::Close };
    for (int i = 0; i < 3; ++i)
    {
        const SDL_FRect& rect = windowButtons[i];
        const bool hovered = mouseX >= rect.x && mouseX < rect.x + rect.w &&
            mouseY >= rect.y && mouseY < rect.y + rect.h;
        DrawRaisedButton(renderer, rect, hovered && leftDown);
        DrawWindowGlyph(renderer, rect, buttonKinds[i]);
    }

    // ---- Menu bar --------------------------------------------------------
    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    const SDL_FRect menuBar{ 0.0f, TitleBarHeight, static_cast<float>(windowWidth),
        MenuBarHeight };
    SDL_RenderFillRect(renderer, &menuBar);
    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(renderer, 0.0f, ChromeHeight - 2.0f,
        static_cast<float>(windowWidth), ChromeHeight - 2.0f);
    Win95Theme::SetRenderColor(renderer, Win95Theme::Shadow);
    SDL_RenderLine(renderer, 0.0f, ChromeHeight - 1.0f,
        static_cast<float>(windowWidth), ChromeHeight - 1.0f);

    SDL_FRect menuItems[kMenuCount];
    LayoutMenuItems(menuItems);
    for (int i = 0; i < kMenuCount; ++i)
    {
        const SDL_FRect& rect = menuItems[i];
        const bool hovered = mouseX >= rect.x && mouseX < rect.x + rect.w &&
            mouseY >= rect.y && mouseY < rect.y + rect.h;
        if (hovered)
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::SelectedItem);
            SDL_RenderFillRect(renderer, &rect);
            Win95Theme::SetRenderColor(renderer, Win95Theme::SelectedItemText);
        }
        else
        {
            Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        }
        UiFont_DrawText(renderer, rect.x + kMenuPadX * 0.5f, rect.y + 2.0f,
            kMenuTextSize, kMenuItems[i]);
    }
}

FrontendChrome::WindowButton FrontendChrome::HitTestWindowButton(
    int windowWidth, float x, float y)
{
    SDL_FRect windowButtons[3];
    LayoutWindowButtons(windowWidth, windowButtons);
    const WindowButton kinds[3] = {
        WindowButton::Minimize, WindowButton::Maximize, WindowButton::Close };
    for (int i = 0; i < 3; ++i)
    {
        const SDL_FRect& rect = windowButtons[i];
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h)
            return kinds[i];
    }
    return WindowButton::None;
}

int FrontendChrome::MenuItemAt(int windowWidth, float x, float y)
{
    (void)windowWidth;
    SDL_FRect menuItems[kMenuCount];
    LayoutMenuItems(menuItems);
    for (int i = 0; i < kMenuCount; ++i)
    {
        const SDL_FRect& rect = menuItems[i];
        if (x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h)
            return i;
    }
    return -1;
}

bool FrontendChrome::IsInTitleBar(float x, float y)
{
    (void)x;
    return y >= 0.0f && y < TitleBarHeight;
}

void FrontendChrome::Shutdown()
{
    DestroyIcon();
}
