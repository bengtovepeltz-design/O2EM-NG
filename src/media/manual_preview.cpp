#include "src/media/manual_preview.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

namespace
{
    using FPDF_DOCUMENT = void*;
    using FPDF_PAGE = void*;
    using FPDF_BITMAP = void*;

    constexpr int FPDF_ANNOT = 0x01;
    constexpr int FPDF_LCD_TEXT = 0x02;

    struct PdfiumApi
    {
        HMODULE module = nullptr;
        void(__cdecl* InitLibrary)() = nullptr;
        void(__cdecl* DestroyLibrary)() = nullptr;
        FPDF_DOCUMENT(__cdecl* LoadDocument)(const char*, const char*) = nullptr;
        void(__cdecl* CloseDocument)(FPDF_DOCUMENT) = nullptr;
        int(__cdecl* GetPageCount)(FPDF_DOCUMENT) = nullptr;
        FPDF_PAGE(__cdecl* LoadPage)(FPDF_DOCUMENT, int) = nullptr;
        void(__cdecl* ClosePage)(FPDF_PAGE) = nullptr;
        double(__cdecl* GetPageWidth)(FPDF_PAGE) = nullptr;
        double(__cdecl* GetPageHeight)(FPDF_PAGE) = nullptr;
        FPDF_BITMAP(__cdecl* BitmapCreate)(int, int, int) = nullptr;
        void(__cdecl* BitmapDestroy)(FPDF_BITMAP) = nullptr;
        void* (__cdecl* BitmapGetBuffer)(FPDF_BITMAP) = nullptr;
        int(__cdecl* BitmapGetStride)(FPDF_BITMAP) = nullptr;
        void(__cdecl* BitmapFillRect)(FPDF_BITMAP, int, int, int, int, unsigned long) = nullptr;
        void(__cdecl* RenderPageBitmap)(FPDF_BITMAP, FPDF_PAGE, int, int, int, int, int, int) = nullptr;

        bool loaded = false;
        bool attempted = false;
    };

    PdfiumApi gPdfium;
    SDL_Texture* gTexture = nullptr;
    SDL_Renderer* gRenderer = nullptr;
    std::wstring gPath;

    template<typename T>
    bool LoadFunction(T& target, const char* name)
    {
        target = reinterpret_cast<T>(GetProcAddress(gPdfium.module, name));
        return target != nullptr;
    }

    std::filesystem::path ExecutableDirectory()
    {
        std::vector<wchar_t> buffer(32768);
        const DWORD length = GetModuleFileNameW(
            nullptr,
            buffer.data(),
            static_cast<DWORD>(buffer.size()));

        if (length == 0 || length >= buffer.size())
            return {};

        return std::filesystem::path(
            std::wstring(buffer.data(), length)).parent_path();
    }

    bool EnsurePdfiumLoaded()
    {
        if (gPdfium.attempted)
            return gPdfium.loaded;

        gPdfium.attempted = true;

        const auto dllPath = ExecutableDirectory() / L"pdfium.dll";
        gPdfium.module = LoadLibraryW(dllPath.c_str());
        if (!gPdfium.module)
            gPdfium.module = LoadLibraryW(L"pdfium.dll");
        if (!gPdfium.module)
            return false;

        const bool ok =
            LoadFunction(gPdfium.InitLibrary, "FPDF_InitLibrary") &&
            LoadFunction(gPdfium.DestroyLibrary, "FPDF_DestroyLibrary") &&
            LoadFunction(gPdfium.LoadDocument, "FPDF_LoadDocument") &&
            LoadFunction(gPdfium.CloseDocument, "FPDF_CloseDocument") &&
            LoadFunction(gPdfium.GetPageCount, "FPDF_GetPageCount") &&
            LoadFunction(gPdfium.LoadPage, "FPDF_LoadPage") &&
            LoadFunction(gPdfium.ClosePage, "FPDF_ClosePage") &&
            LoadFunction(gPdfium.GetPageWidth, "FPDF_GetPageWidth") &&
            LoadFunction(gPdfium.GetPageHeight, "FPDF_GetPageHeight") &&
            LoadFunction(gPdfium.BitmapCreate, "FPDFBitmap_Create") &&
            LoadFunction(gPdfium.BitmapDestroy, "FPDFBitmap_Destroy") &&
            LoadFunction(gPdfium.BitmapGetBuffer, "FPDFBitmap_GetBuffer") &&
            LoadFunction(gPdfium.BitmapGetStride, "FPDFBitmap_GetStride") &&
            LoadFunction(gPdfium.BitmapFillRect, "FPDFBitmap_FillRect") &&
            LoadFunction(gPdfium.RenderPageBitmap, "FPDF_RenderPageBitmap");

        if (!ok)
        {
            FreeLibrary(gPdfium.module);
            gPdfium.module = nullptr;
            return false;
        }

        gPdfium.InitLibrary();
        gPdfium.loaded = true;
        return true;
    }

    std::string WideToUtf8(const std::wstring& text)
    {
        if (text.empty())
            return {};

        const int required = WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            nullptr, 0, nullptr, nullptr);
        if (required <= 0)
            return {};

        std::string result(static_cast<std::size_t>(required), '\0');
        WideCharToMultiByte(
            CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
            result.data(), required, nullptr, nullptr);
        return result;
    }

    void DestroyTexture()
    {
        if (gTexture)
        {
            SDL_DestroyTexture(gTexture);
            gTexture = nullptr;
        }
    }

    SDL_Texture* RenderFirstPage(
        SDL_Renderer* renderer,
        const std::filesystem::path& path)
    {
        if (!renderer || !EnsurePdfiumLoaded())
            return nullptr;

        const std::string utf8Path = WideToUtf8(path.wstring());
        if (utf8Path.empty())
            return nullptr;

        FPDF_DOCUMENT document = gPdfium.LoadDocument(utf8Path.c_str(), nullptr);
        if (!document)
            return nullptr;

        if (gPdfium.GetPageCount(document) < 1)
        {
            gPdfium.CloseDocument(document);
            return nullptr;
        }

        FPDF_PAGE page = gPdfium.LoadPage(document, 0);
        if (!page)
        {
            gPdfium.CloseDocument(document);
            return nullptr;
        }

        const double pageWidth = gPdfium.GetPageWidth(page);
        const double pageHeight = gPdfium.GetPageHeight(page);
        if (pageWidth <= 0.0 || pageHeight <= 0.0)
        {
            gPdfium.ClosePage(page);
            gPdfium.CloseDocument(document);
            return nullptr;
        }

        constexpr int maxWidth = 1200;
        constexpr int maxHeight = 1600;
        const double scale = (std::min)(
            static_cast<double>(maxWidth) / pageWidth,
            static_cast<double>(maxHeight) / pageHeight);

        const int bitmapWidth = (std::max)(1, static_cast<int>(pageWidth * scale + 0.5));
        const int bitmapHeight = (std::max)(1, static_cast<int>(pageHeight * scale + 0.5));

        FPDF_BITMAP bitmap = gPdfium.BitmapCreate(bitmapWidth, bitmapHeight, 1);
        if (!bitmap)
        {
            gPdfium.ClosePage(page);
            gPdfium.CloseDocument(document);
            return nullptr;
        }

        gPdfium.BitmapFillRect(
            bitmap, 0, 0, bitmapWidth, bitmapHeight, 0xFFFFFFFFul);
        gPdfium.RenderPageBitmap(
            bitmap, page, 0, 0, bitmapWidth, bitmapHeight, 0,
            FPDF_ANNOT | FPDF_LCD_TEXT);

        void* pixels = gPdfium.BitmapGetBuffer(bitmap);
        const int stride = gPdfium.BitmapGetStride(bitmap);

        SDL_Texture* texture = nullptr;
        if (pixels && stride > 0)
        {
            texture = SDL_CreateTexture(
                renderer,
                SDL_PIXELFORMAT_BGRA32,
                SDL_TEXTUREACCESS_STATIC,
                bitmapWidth,
                bitmapHeight);

            if (texture)
            {
                if (!SDL_UpdateTexture(texture, nullptr, pixels, stride))
                {
                    SDL_DestroyTexture(texture);
                    texture = nullptr;
                }
                else
                {
                    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_LINEAR);
                }
            }
        }

        gPdfium.BitmapDestroy(bitmap);
        gPdfium.ClosePage(page);
        gPdfium.CloseDocument(document);
        return texture;
    }

    void EnsureTexture(SDL_Renderer* renderer, const std::filesystem::path& path)
    {
        const std::wstring newPath = path.wstring();
        if (renderer == gRenderer && newPath == gPath)
            return;

        DestroyTexture();
        gRenderer = renderer;
        gPath = newPath;

        if (!renderer || path.empty() || !std::filesystem::exists(path))
            return;

        gTexture = RenderFirstPage(renderer, path);
    }
}

bool ManualPreview_Draw(
    SDL_Renderer* renderer,
    const std::filesystem::path& pdfPath,
    const SDL_FRect& destination)
{
    EnsureTexture(renderer, pdfPath);
    if (!gTexture)
        return false;

    float imageWidth = 0.0f;
    float imageHeight = 0.0f;
    if (!SDL_GetTextureSize(gTexture, &imageWidth, &imageHeight) ||
        imageWidth <= 0.0f || imageHeight <= 0.0f)
    {
        return false;
    }

    const float scale = (std::min)(
        destination.w / imageWidth,
        destination.h / imageHeight);

    const SDL_FRect target{
        destination.x + (destination.w - imageWidth * scale) * 0.5f,
        destination.y + (destination.h - imageHeight * scale) * 0.5f,
        imageWidth * scale,
        imageHeight * scale
    };

    return SDL_RenderTexture(renderer, gTexture, nullptr, &target);
}

void ManualPreview_Shutdown()
{
    DestroyTexture();
    gRenderer = nullptr;
    gPath.clear();

    if (gPdfium.loaded)
    {
        gPdfium.DestroyLibrary();
        gPdfium.loaded = false;
    }
    if (gPdfium.module)
    {
        FreeLibrary(gPdfium.module);
        gPdfium.module = nullptr;
    }
    gPdfium.attempted = false;
}
