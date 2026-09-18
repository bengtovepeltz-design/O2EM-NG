#pragma once

#include <SDL3/SDL.h>
#include <filesystem>

struct GameInfo;

void FrontendScreenshot_Draw(
    SDL_Renderer* renderer,
    const SDL_FRect& rightContent,
    const GameInfo* game,
    bool compact = false);

// Viewer-mode entry points for embedding the media viewer inside another
// panel (Library Cover / Media). Same media engine, custom footer height.
void FrontendScreenshot_SetCompact(bool compact);
void FrontendScreenshot_ResetView();
void FrontendScreenshot_SetCompactIndex(std::size_t index);

// Moves to the previous/next screenshot. The index wraps around.
void FrontendScreenshot_Move(const GameInfo* game, int direction);

// Handles the Previous/Next buttons drawn by the screenshot module.
bool FrontendScreenshot_HitTest(
    const SDL_FRect& rightContent,
    const GameInfo* game,
    float x,
    float y,
    bool compact = false);


// Returns true when the visible Delete button is clicked.
bool FrontendScreenshot_DeleteHitTest(
    const SDL_FRect& rightContent,
    const GameInfo* game,
    float x,
    float y,
    bool compact = false);

// Returns the screenshot currently displayed, or an empty path.
std::filesystem::path FrontendScreenshot_CurrentPath(const GameInfo* game);

// Clears cached texture/index after screenshots are imported or removed.
void FrontendScreenshot_Invalidate();

void FrontendScreenshot_Shutdown();

// Advances visible GIFs; returns true when the UI needs repainting.
bool FrontendScreenshot_Update(const GameInfo* game, bool visible);
