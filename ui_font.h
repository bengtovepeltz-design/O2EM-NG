#pragma once

#include <SDL3/SDL.h>

#include <string>

// Draws ordinary Windows UI text using an installed system font.
// The font is loaded lazily from the Windows Fonts directory.
bool UiFont_DrawText(
    SDL_Renderer* renderer,
    float x,
    float y,
    float pointSize,
    const std::string& text);

void UiFont_Shutdown();
