#pragma once

#include <SDL3/SDL.h>
#include <filesystem>

struct GameInfo;

void FrontendScreenshot_Draw(
    SDL_Renderer* renderer,
    const SDL_FRect& rightContent,
    const GameInfo* game);

// Moves to the previous/next screenshot. The index wraps around.
void FrontendScreenshot_Move(const GameInfo* game, int direction);

// Handles the Previous/Next buttons drawn by the screenshot module.
bool FrontendScreenshot_HitTest(
    const SDL_FRect& rightContent,
    const GameInfo* game,
    float x,
    float y);


// Returns true when the visible Delete button is clicked.
bool FrontendScreenshot_DeleteHitTest(
    const SDL_FRect& rightContent,
    const GameInfo* game,
    float x,
    float y);

// Returns the screenshot currently displayed, or an empty path.
std::filesystem::path FrontendScreenshot_CurrentPath(const GameInfo* game);

// Clears cached texture/index after screenshots are imported or removed.
void FrontendScreenshot_Invalidate();

void FrontendScreenshot_Shutdown();
