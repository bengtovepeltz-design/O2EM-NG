#include "frontend_layout.h"
#include "frontend_chrome.h"
#include "theme_win95.h"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <string>

namespace
{
    SDL_Texture* gHeaderTexture = nullptr;
    SDL_Renderer* gHeaderRenderer = nullptr;

    void DrawHeaderText(
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

    void DestroyHeaderTexture()
    {
        if (gHeaderTexture)
        {
            SDL_DestroyTexture(gHeaderTexture);
            gHeaderTexture = nullptr;
        }
        gHeaderRenderer = nullptr;
    }

    void EnsureHeaderTexture(SDL_Renderer* renderer)
    {
        if (renderer != gHeaderRenderer)
        {
            DestroyHeaderTexture();
            gHeaderRenderer = renderer;
        }

        if (gHeaderTexture)
            return;

        const char* basePath = SDL_GetBasePath();
        const std::string imagePath =
            std::string(basePath ? basePath : "") + "assets/O2EM-NG_Header.png";
        gHeaderTexture = IMG_LoadTexture(renderer, imagePath.c_str());
        if (!gHeaderTexture)
        {
            // Renderer recreation after returning from emulation can briefly
            // make the first image load fail. Do not latch that failure; the
            // next frontend redraw must be allowed to try again.
            SDL_Log("O2EM-NG: header reload failed, will retry: %s", SDL_GetError());
        }
    }
}

void FrontendLayout_DrawHeader(SDL_Window* window, SDL_Renderer* renderer)
{
    if (!window || !renderer)
        return;

    int windowW = 0;
    int windowH = 0;
    SDL_GetWindowSize(window, &windowW, &windowH);
    (void)windowH;

    const float margin = FrontendChrome::BannerMargin;
    const float bx = margin;
    const float by = FrontendChrome::BannerTop;
    const float bw = static_cast<float>(windowW) - margin * 2.0f;
    const float bh = FrontendChrome::BannerHeight;
    if (bw <= 8.0f)
        return;

    // Gray application background between the chrome and the banner frame.
    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    const SDL_FRect background{
        0.0f, FrontendChrome::ChromeHeight,
        static_cast<float>(windowW),
        (by + bh + margin) - FrontendChrome::ChromeHeight };
    SDL_RenderFillRect(renderer, &background);

    // Classic Win95 raised bevel around the banner.
    const SDL_FRect frame{ bx, by, bw, bh };
    Win95Theme::SetRenderColor(renderer, Win95Theme::Face);
    SDL_RenderFillRect(renderer, &frame);
    Win95Theme::SetRenderColor(renderer, Win95Theme::Highlight);
    SDL_RenderLine(renderer, frame.x, frame.y, frame.x + frame.w - 1.0f, frame.y);
    SDL_RenderLine(renderer, frame.x, frame.y, frame.x, frame.y + frame.h - 1.0f);
    Win95Theme::SetRenderColor(renderer, Win95Theme::DarkShadow);
    SDL_RenderLine(renderer, frame.x, frame.y + frame.h - 1.0f,
        frame.x + frame.w - 1.0f, frame.y + frame.h - 1.0f);
    SDL_RenderLine(renderer, frame.x + frame.w - 1.0f, frame.y,
        frame.x + frame.w - 1.0f, frame.y + frame.h - 1.0f);

    // Inner media area (black) so the artwork blends into the frame.
    const SDL_FRect inner{ frame.x + 3.0f, frame.y + 3.0f,
        frame.w - 6.0f, frame.h - 6.0f };
    SDL_SetRenderDrawColor(renderer, 4, 8, 12, 255);
    SDL_RenderFillRect(renderer, &inner);

    EnsureHeaderTexture(renderer);
    if (gHeaderTexture)
    {
        float textureW = 0.0f;
        float textureH = 0.0f;
        if (SDL_GetTextureSize(gHeaderTexture, &textureW, &textureH) &&
            textureW > 0.0f && textureH > 0.0f)
        {
            // The supplied artwork is a complete banner composition. Stretch
            // it to the framed inner rectangle so no logo or text is cropped
            // when switching between windowed and fullscreen modes.
            const SDL_FRect destination = inner;
            SDL_RenderTexture(renderer, gHeaderTexture, nullptr, &destination);

            // The original low banner contains a legacy/garbled label after
            // the final word PRESERVATION. Keep the original artwork intact
            // and cover only that obsolete label at render time. Coordinates
            // are proportional to the source artwork, so this works at any
            // banner size without clipping PRESERVATION.
            constexpr float sourceWidth = 1496.0f;
            constexpr float sourceHeight = 179.0f;
            constexpr float maskSourceX = 1068.0f;
            constexpr float maskSourceY = 156.0f;
            constexpr float maskSourceW = 205.0f;
            constexpr float maskSourceH = 20.0f;

            const float scaleX = destination.w / sourceWidth;
            const float scaleY = destination.h / sourceHeight;
            const SDL_FRect obsoleteLabelMask{
                destination.x + maskSourceX * scaleX,
                destination.y + maskSourceY * scaleY,
                maskSourceW * scaleX,
                maskSourceH * scaleY
            };

            SDL_SetRenderDrawColor(renderer, 2, 3, 3, 255);
            SDL_RenderFillRect(renderer, &obsoleteLabelMask);
        }
    }
    else
    {
        // Safe fallback when the optional artwork cannot be loaded.
        SDL_SetRenderDrawColor(renderer, 20, 55, 112, 255);
        SDL_RenderFillRect(renderer, &inner);
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        DrawHeaderText(renderer, inner.x + 20.0f, inner.y + 22.0f, 2.2f, "O2EM-NG");
        DrawHeaderText(renderer, inner.x + 20.0f, inner.y + 64.0f, 1.25f,
            "PHILIPS VIDEOPAC G7000 - CLASSIC GAMING COLLECTION");
    }
}

void FrontendLayout_Shutdown()
{
    DestroyHeaderTexture();
}
