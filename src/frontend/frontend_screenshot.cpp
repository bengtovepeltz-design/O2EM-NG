#include "src/frontend/frontend_screenshot.h"
#include "theme_win95.h"
#include "src/media/video_preview.h"
#include "src/library/game_info.h"

#include <SDL3_image/SDL_image.h>
#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>
#include <cctype>

namespace
{
    std::unique_ptr<VideoPreview> gVideo;
    bool gVideoFailed = false;
    IMG_Animation* gAnimation = nullptr;
    std::vector<Uint64> gFrameEnds;
    Uint64 gAnimationStart = 0;
    int gAnimationFrame = 0;
    SDL_Texture* gTexture = nullptr;
    SDL_Renderer* gRenderer = nullptr;
    std::string gScreenshotKey;
    std::string gGameKey;
    std::size_t gScreenshotIndex = 0;
    // Compact embedding (Library Cover / Media): suppresses the module's own
    // frame/title header and shrinks the footer; media engine unchanged.
    bool gCompact = false;
    float gCompactFooter = 0.0f;
    std::size_t gCompactRestoreIndex = 0;
    bool gCompactRestoreValid = false;

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
        gVideo.reset();
        gVideoFailed = false;
        if (gAnimation) IMG_FreeAnimation(gAnimation);
        gAnimation = nullptr;
        gFrameEnds.clear();
        gAnimationFrame = 0;
        if (gTexture)
        {
            SDL_DestroyTexture(gTexture);
            gTexture = nullptr;
        }
    }

    std::string GameKey(const GameInfo* game)
    {
        if (!game) return {};
        // Filename/Catalog ID are stable identities for Plus, prototype and
        // alphanumeric entries. videopacNumber+title can collide or stay at 0.
        return game->filename + "|" + game->catalogId + "|" + game->title;
    }

    void SynchronizeGame(const GameInfo* game)
    {
        const std::string key = GameKey(game);
        if (key != gGameKey)
        {
            gGameKey = key;
            gScreenshotIndex = 0;
            // Embedded Library viewer: keep showing the media item that was
            // current when the app last remembered it (game switch restores
            // the same position instead of jumping back to the first item).
            if (gCompact && gCompactRestoreValid && game &&
                gCompactRestoreIndex < game->screenshots.size())
            {
                gScreenshotIndex = gCompactRestoreIndex;
            }
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
        const auto& path = game->screenshots[gScreenshotIndex];
        std::string extension = path.extension().string();
        std::transform(extension.begin(), extension.end(), extension.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (extension == ".mp4")
        {
            gVideo = std::make_unique<VideoPreview>(path);
            return;
        }
        if (extension == ".gif")
        {
            gAnimation = IMG_LoadAnimation(path.string().c_str());
            if (gAnimation && gAnimation->count > 0 && gAnimation->frames && gAnimation->delays)
            {
                Uint64 total = 0;
                for (int i = 0; i < gAnimation->count; ++i)
                {
                    total += gAnimation->delays[i] > 0 ? gAnimation->delays[i] : 100;
                    gFrameEnds.push_back(total);
                }
                gAnimationStart = SDL_GetTicks();
                gTexture = SDL_CreateTextureFromSurface(renderer, gAnimation->frames[0]);
                if (gTexture) return;
            }
            DestroyTexture();
        }
        gTexture = IMG_LoadTexture(renderer, path.string().c_str());
    }

    void ButtonRects(const SDL_FRect& rightContent, SDL_FRect& previous,
        SDL_FRect& next, bool compact = false)
    {
        const float margin = 14.0f;
        const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
            rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
        const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
            frame.w - 8.0f, frame.h - 8.0f};
        const float rowY = compact
            ? inner.y + inner.h - 30.0f
            : inner.y + inner.h - 45.0f;
        const float buttonH = compact ? 26.0f : 30.0f;
        previous = {inner.x + 20.0f, rowY, 116.0f, buttonH};
        next = {inner.x + inner.w - 136.0f, rowY, 116.0f, buttonH};
    }

    SDL_FRect DeleteButtonRect(const SDL_FRect& rightContent, bool compact = false)
    {
        const float margin = 14.0f;
        const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
            rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
        const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
            frame.w - 8.0f, frame.h - 8.0f};
        if (compact)
        {
            // Embedded Library viewer: own compact row below PREVIOUS/NEXT;
            // the full-width 150 px button would overlap NEXT at this width.
            return {inner.x + (inner.w - 90.0f) * 0.5f,
                inner.y + inner.h - 30.0f, 90.0f, 26.0f};
        }
        const float rowY = inner.y + inner.h - 45.0f;
        return {inner.x + (inner.w - 150.0f) * 0.5f,
            rowY, 150.0f, 30.0f};
    }

    bool Contains(const SDL_FRect& rect, float x, float y)
    {
        return x >= rect.x && x < rect.x + rect.w &&
            y >= rect.y && y < rect.y + rect.h;
    }
}

void FrontendScreenshot_SetCompact(bool compact)
{
    gCompact = compact;
}

void FrontendScreenshot_ResetView()
{
    // Leaving media mode: stop GIF/MP4 playback and drop the decoded media
    // texture; the media collection index itself is remembered.
    DestroyTexture();
    gScreenshotKey.clear();
}

void FrontendScreenshot_SetCompactIndex(std::size_t index)
{
    gCompactRestoreIndex = index;
    gCompactRestoreValid = true;
}

void FrontendScreenshot_Draw(SDL_Renderer* renderer,
    const SDL_FRect& rightContent, const GameInfo* game)
{
    FrontendScreenshot_Draw(renderer, rightContent, game, false);
}

void FrontendScreenshot_Draw(SDL_Renderer* renderer,
    const SDL_FRect& rightContent, const GameInfo* game, bool compact)
{
    if (!renderer) return;
    gCompact = compact;
    gCompactFooter = 0.0f;
    EnsureTexture(renderer, game);

    const float margin = 14.0f;
    const SDL_FRect frame{rightContent.x + margin, rightContent.y + margin,
        rightContent.w - margin * 2.0f, rightContent.h - margin * 2.0f};
    const SDL_FRect inner{frame.x + 4.0f, frame.y + 4.0f,
        frame.w - 8.0f, frame.h - 8.0f};
    if (!compact)
    {
        DrawSunkenFrame(renderer, frame);
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
    }
    else
    {
        Win95Theme::SetRenderColor(renderer, Win95Theme::Window);
        SDL_RenderFillRect(renderer, &inner);
        Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
        if (!game)
        {
            DrawText(renderer, inner.x + 20.0f, inner.y + 24.0f, 1.0f, "SELECT A GAME");
            return;
        }
    }
    if (game->screenshots.empty())
    {
        if (compact)
        {
            DrawText(renderer, inner.x + 20.0f, inner.y + 24.0f, 1.0f,
                "NO MEDIA AVAILABLE");
            DrawText(renderer, inner.x + 20.0f, inner.y + 50.0f, 0.9f,
                "Use Import Center > Add Screenshot to import one.");
        }
        else
        {
            DrawText(renderer, inner.x + 20.0f, inner.y + 98.0f, 1.0f,
                "NO SCREENSHOTS AVAILABLE");
            DrawText(renderer, inner.x + 20.0f, inner.y + 124.0f, 0.9f,
                "Use Import Center > Add Screenshot to import one.");
        }
        return;
    }

    const float footerHeight = compact ? 62.0f : 62.0f;
    const float contentTop = compact ? 12.0f : 88.0f;
    const SDL_FRect imageArea{inner.x + 20.0f, inner.y + contentTop,
        inner.w - 40.0f, inner.h - contentTop - footerHeight};
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
            gVideo && !gVideoFailed ? "LOADING VIDEO..." : "MEDIA COULD NOT BE LOADED");
    }

    SDL_FRect previous{}, next{};
    ButtonRects(rightContent, previous, next, compact);
    if (game->screenshots.size() > 1)
    {
        DrawRaisedButton(renderer, previous, "< PREVIOUS");
        DrawRaisedButton(renderer, next, "NEXT >");
    }
    // Per-item DELETE in both layouts; deletes only the displayed file.
    DrawRaisedButton(renderer, DeleteButtonRect(rightContent, compact), "DELETE");

    Win95Theme::SetRenderColor(renderer, Win95Theme::WindowText);
    const std::string filename =
        game->screenshots[gScreenshotIndex].filename().string();
    std::string counter = "Screenshot " +
        std::to_string(gScreenshotIndex + 1) + " of " +
        std::to_string(game->screenshots.size());
    // Full tab width fits the filename too; the compact embedded viewer
    // only has the gap between PREVIOUS/NEXT, so keep the count only
    // (the media type itself is visible live in the viewer).
    if (!compact)
        counter += "  -  " + filename;
    if (compact)
        // Own line above the button row (footer grew to two lines).
        DrawText(renderer, inner.x + 20.0f, inner.y + inner.h - 54.0f,
            0.8f, counter);
    else
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
    // Embedded Library viewer: remember the browsed position so switching
    // away and back (or changing games) resumes here.
    if (gCompact)
    {
        gCompactRestoreIndex = gScreenshotIndex;
        gCompactRestoreValid = true;
    }
}

bool FrontendScreenshot_HitTest(const SDL_FRect& rightContent,
    const GameInfo* game, float x, float y, bool compact)
{
    if (!game || game->screenshots.size() < 2) return false;
    SDL_FRect previous{}, next{};
    ButtonRects(rightContent, previous, next, compact);
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
    const GameInfo* game, float x, float y, bool compact)
{
    if (!game || game->screenshots.empty()) return false;
    return Contains(DeleteButtonRect(rightContent, compact), x, y);
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
    gCompact = false;
    gCompactFooter = 0.0f;
    gCompactRestoreValid = false;
    gCompactRestoreIndex = 0;
}

// Called from the UI tick, including when no input requests a redraw.
bool FrontendScreenshot_Update(const GameInfo* game, bool visible)
{
    if (!visible)
    {
        if (gAnimation || gVideo) FrontendScreenshot_Invalidate();
        return false;
    }
    SynchronizeGame(game);
    if (gVideo)
    {
        const bool failed = gVideo->Failed();
        const bool changed = failed != gVideoFailed;
        gVideoFailed = failed;
        return gVideo->Update(gRenderer, gTexture) || changed;
    }
    if (!gAnimation || gFrameEnds.size() < 2) return false;
    const Uint64 elapsed = (SDL_GetTicks() - gAnimationStart) % gFrameEnds.back();
    const int next = static_cast<int>(std::upper_bound(gFrameEnds.begin(), gFrameEnds.end(), elapsed) - gFrameEnds.begin());
    if (next == gAnimationFrame) return false;
    SDL_Texture* texture = SDL_CreateTextureFromSurface(gRenderer, gAnimation->frames[next]);
    if (!texture) return false;
    SDL_DestroyTexture(gTexture);
    gTexture = texture;
    gAnimationFrame = next;
    return true;
}
