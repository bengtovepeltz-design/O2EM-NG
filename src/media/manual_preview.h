#pragma once

#include <SDL3/SDL.h>
#include <filesystem>

// Renders page 1 of the selected PDF with PDFium and draws it fitted inside
// destination. The rendered SDL texture is cached until another manual is
// selected. pdfium.dll must be next to O2EM-NG.exe.
bool ManualPreview_Draw(
    SDL_Renderer* renderer,
    const std::filesystem::path& pdfPath,
    const SDL_FRect& destination);

void ManualPreview_Shutdown();
