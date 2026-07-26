#include "frontend_layout.h"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <string>

namespace
{
    SDL_Texture* gHeaderTexture = nullptr;
    SDL_Renderer* gHeaderRenderer = nullptr;
    bool gHeaderLoadAttempted = false;

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
        gHeaderLoadAttempted = false;
    }

    void EnsureHeaderTexture(SDL_Renderer* renderer)
    {
        if (renderer != gHeaderRenderer)
        {
            DestroyHeaderTexture();
            gHeaderRenderer = renderer;
        }

        if (gHeaderTexture || gHeaderLoadAttempted)
            return;

        gHeaderLoadAttempted = true;
        const char* basePath = SDL_GetBasePath();
        const std::string imagePath =
            std::string(basePath ? basePath : "") + "assets/O2EM-NG_Header.png";
        gHeaderTexture = IMG_LoadTexture(renderer, imagePath.c_str());
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

    constexpr float headerHeight = 142.0f;
    EnsureHeaderTexture(renderer);

    SDL_SetRenderDrawColor(renderer, 4, 8, 12, 255);
    const SDL_FRect header{ 0.0f, 0.0f, static_cast<float>(windowW), headerHeight };
    SDL_RenderFillRect(renderer, &header);

    if (gHeaderTexture)
    {
        float textureW = 0.0f;
        float textureH = 0.0f;
        if (SDL_GetTextureSize(gHeaderTexture, &textureW, &textureH) &&
            textureW > 0.0f && textureH > 0.0f)
        {
            // The supplied artwork is a complete banner composition. Stretch
            // it to the responsive header rectangle so no logo or text is cropped
            // when switching between windowed and fullscreen modes. The area
            // behind it remains pure black, so the artwork blends into the GUI.
            const SDL_FRect destination{
                0.0f,
                0.0f,
                static_cast<float>(windowW),
                headerHeight
            };
            SDL_RenderTexture(renderer, gHeaderTexture, nullptr, &destination);

            // The original low banner contains a legacy/garbled label after
            // the final word PRESERVATION. Keep the original artwork intact
            // and cover only that obsolete label at render time. Coordinates
            // are proportional to the source artwork, so this works in both
            // windowed and fullscreen modes without clipping PRESERVATION.
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
        SDL_RenderFillRect(renderer, &header);
        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        DrawHeaderText(renderer, 30.0f, 30.0f, 2.2f, "O2EM-NG");
        DrawHeaderText(renderer, 30.0f, 72.0f, 1.25f,
            "PHILIPS VIDEOPAC G7000 - CLASSIC GAMING COLLECTION");
    }

}

void FrontendLayout_Shutdown()
{
    DestroyHeaderTexture();
}
