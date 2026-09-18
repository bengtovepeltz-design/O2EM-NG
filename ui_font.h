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

// Same as UiFont_DrawText but using the bold face of the same system font
// (falls back to the regular face with a synthetic bold style).
bool UiFont_DrawTextBold(
    SDL_Renderer* renderer,
    float x,
    float y,
    float pointSize,
    const std::string& text);

// Measures text as rendered by UiFont_DrawText/UiFont_DrawTextBold.
// Returns false when no system font is available; callers keep their
// previous character-count estimate as fallback.
bool UiFont_MeasureText(
    float pointSize,
    const std::string& text,
    float* outWidth,
    float* outHeight,
    bool bold = false);

void UiFont_Shutdown();
