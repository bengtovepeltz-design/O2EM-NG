// ============================================================================
// src/frontend/startup_splash.cpp
//
// See startup_splash.h. Dark, restrained "retro console boot" composition:
// round O2EM-NG badge, release hierarchy (O2EM-NG / release name / version),
// Videopac stripe accent, inset progress bar with real stages, and an
// isolated one-shot playback of the supplied real G7000 reset recording.
// ============================================================================

#include "startup_splash.h"

#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "ui_font.h"
#include "src/version.h"

namespace
{
    // --- palette -----------------------------------------------------------
    const SDL_Color kBgTop      = {   6,  16,  38, 255 };
    const SDL_Color kBgBottom   = {  16,  40,  82, 255 };
    const SDL_Color kCream      = { 244, 236, 214, 255 };
    const SDL_Color kRed        = { 206,  44,  48, 255 };
    const SDL_Color kOrange     = { 230, 138,  34, 255 };
    const SDL_Color kYellow     = { 236, 200,  66, 255 };
    const SDL_Color kBlue       = {  44,  96, 200, 255 };
    const SDL_Color kMuted      = { 158, 182, 214, 255 };
    const SDL_Color kPanelLine  = {  54,  92, 156, 255 };
    const SDL_Color kTrack      = {   5,  12,  26, 255 };
    const SDL_Color kTrackEdge  = {   3,   7,  16, 255 };
    const SDL_Color kFill       = {  34,  96, 206, 255 };
    const SDL_Color kFillHi     = {  96, 156, 246, 255 };

    Uint16 ReadU16LE(const Uint8* p)
    {
        return static_cast<Uint16>(p[0] | (p[1] << 8));
    }

    Uint32 ReadU32LE(const Uint8* p)
    {
        return static_cast<Uint32>(p[0]) |
               (static_cast<Uint32>(p[1]) << 8) |
               (static_cast<Uint32>(p[2]) << 16) |
               (static_cast<Uint32>(p[3]) << 24);
    }

    void SetColor(SDL_Renderer* r, const SDL_Color& c)
    {
        SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
    }

    void Fill(SDL_Renderer* r, const SDL_FRect& rect, const SDL_Color& c)
    {
        SetColor(r, c);
        SDL_RenderFillRect(r, &rect);
    }

    void HRule(SDL_Renderer* r, float x1, float x2, float y, const SDL_Color& c, float thickness = 1.0f)
    {
        SetColor(r, c);
        SDL_FRect rect{ x1, y, x2 - x1, thickness };
        SDL_RenderFillRect(r, &rect);
    }

    void DrawVerticalGradient(SDL_Renderer* r, float w, float h,
        const SDL_Color& top, const SDL_Color& bottom, int bands)
    {
        if (bands < 2)
            bands = 2;
        for (int i = 0; i < bands; ++i)
        {
            const float t = static_cast<float>(i) / static_cast<float>(bands - 1);
            const SDL_Color c{
                static_cast<Uint8>(top.r + (bottom.r - top.r) * t),
                static_cast<Uint8>(top.g + (bottom.g - top.g) * t),
                static_cast<Uint8>(top.b + (bottom.b - top.b) * t),
                255 };
            const float y = h * static_cast<float>(i) / static_cast<float>(bands);
            const float bandH = h / static_cast<float>(bands) + 1.0f;
            SDL_FRect row{ 0.0f, y, w, bandH };
            SetColor(r, c);
            SDL_RenderFillRect(r, &row);
        }
    }

    void DrawCenteredText(SDL_Renderer* r, float cx, float y, float size,
        const std::string& text, bool bold, const SDL_Color& color)
    {
        float w = 0.0f, h = 0.0f;
        if (!UiFont_MeasureText(size, text, &w, &h, bold))
            return;
        SetColor(r, color);
        if (bold)
            UiFont_DrawTextBold(r, cx - w * 0.5f, y, size, text);
        else
            UiFont_DrawText(r, cx - w * 0.5f, y, size, text);
    }

    void DrawRightText(SDL_Renderer* r, float rightX, float y, float size,
        const std::string& text, bool bold, const SDL_Color& color)
    {
        float w = 0.0f, h = 0.0f;
        if (!UiFont_MeasureText(size, text, &w, &h, bold))
            return;
        SetColor(r, color);
        if (bold)
            UiFont_DrawTextBold(r, rightX - w, y, size, text);
        else
            UiFont_DrawText(r, rightX - w, y, size, text);
    }

    // Thin 4-stripe Videopac accent used as a divider.
    void DrawStripeDivider(SDL_Renderer* r, float x, float y, float w, float h)
    {
        const SDL_Color cols[4] = { kRed, kOrange, kYellow, kBlue };
        const float seg = w / 4.0f;
        for (int i = 0; i < 4; ++i)
        {
            SDL_FRect rect{ x + seg * i, y, seg + 0.5f, h };
            Fill(r, rect, cols[i]);
        }
    }
}

StartupSplash::StartupSplash(SDL_Window* window, const char* productName, const char* version)
    : window_(window)
    , productName_(productName ? productName : "O2EM-NG")
    , version_(version ? version : "")
{
    if (window_)
        renderer_ = SDL_GetRenderer(window_);
    startTicks_ = SDL_GetTicks();
}

StartupSplash::~StartupSplash()
{
    CloseAudio();
    if (logo_)
        SDL_DestroyTexture(logo_);
}

void StartupSplash::SetLogoPath(const std::string& path)
{
    if (path == logoPath_)
        return;
    logoPath_ = path;
    logoAttempted_ = false;
}

void StartupSplash::SetStageCount(int count)
{
    stageCount_ = std::max(0, count);
}

void StartupSplash::Stage(const char* label, int completed)
{
    if (label)
        stageLabel_ = label;
    stageCompleted_ = std::max(0, completed);
    Draw();
    Present();
    PumpEvents();
}

void StartupSplash::Show()
{
    if (window_)
        SDL_ShowWindow(window_);
    // Present once after the window is visible so the first visible frame is
    // the painted splash, never an empty window, then fade it in.
    Draw();
    Present();
    FadeIn();
}

void StartupSplash::FadeIn()
{
    const Uint64 start = SDL_GetTicks();
    const Uint64 duration = 220;
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_BLEND);
    while (!quitRequested_)
    {
        const Uint64 elapsed = SDL_GetTicks() - start;
        if (elapsed >= duration)
            break;
        Draw();
        const Uint8 a = static_cast<Uint8>(255 - (255 * elapsed) / duration);
        SDL_SetRenderDrawColor(renderer_, 0, 0, 0, a);
        SDL_FRect full{};
        int w = 1, h = 1;
        SDL_GetWindowSize(window_, &w, &h);
        full.w = static_cast<float>(w);
        full.h = static_cast<float>(h);
        SDL_RenderFillRect(renderer_, &full);
        Present();
        PumpEvents();
        SDL_Delay(8);
    }
    SDL_SetRenderDrawBlendMode(renderer_, SDL_BLENDMODE_NONE);
    Draw();
    Present();
}

void StartupSplash::FinishAndPlayStartupSound(const std::string& wavPath)
{
    finished_ = true;
    stageLabel_ = "Ready";

    // Anti-flash minimum: keep the finished splash up for a short, bounded
    // moment so a fast startup does not flicker. This never adds seconds.
    while (!quitRequested_ && (SDL_GetTicks() - startTicks_) < minDisplayMs_)
    {
        Draw();
        Present();
        PumpEvents();
        SDL_Delay(16);
    }

    Draw();
    Present();
    PumpEvents();

    PlayStartupSound(wavPath);
}

void StartupSplash::Tick()
{
    if (!audioStream_)
        return;

    const int queued = SDL_GetAudioStreamQueued(audioStream_);
    if (queued <= 0 || SDL_GetTicks() >= audioStopAt_)
        CloseAudio();
}

void StartupSplash::Abort()
{
    aborted_ = true;
    CloseAudio();
}

void StartupSplash::EnsureLogoTexture()
{
    if (logoAttempted_ || logoPath_.empty() || !renderer_)
        return;
    logoAttempted_ = true;

    logo_ = IMG_LoadTexture(renderer_, logoPath_.c_str());
    if (!logo_)
    {
        std::printf("O2EM-NG: startup logo not available (%s); using text emblem.\n",
            SDL_GetError());
        std::fflush(stdout);
        return;
    }
    SDL_SetTextureBlendMode(logo_, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(logo_, SDL_SCALEMODE_LINEAR);
}

void StartupSplash::Draw()
{
    if (!renderer_ || !window_)
        return;

    int winW = 1, winH = 1;
    SDL_GetWindowSize(window_, &winW, &winH);
    const float W = static_cast<float>(winW);
    const float H = static_cast<float>(winH);

    // Background: restrained dark navy gradient.
    DrawVerticalGradient(renderer_, W, H, kBgTop, kBgBottom, 96);

    // Understated outer hairline frame.
    {
        SetColor(renderer_, kPanelLine);
        SDL_FRect frame{ 14.0f, 14.0f, W - 28.0f, H - 28.0f };
        SDL_RenderRect(renderer_, &frame);
    }

    const float s = SDL_clamp(H / 960.0f, 0.72f, 1.35f);
    const float contentW = SDL_min(720.0f * s, W - 96.0f);
    const float cx = W * 0.5f;

    EnsureLogoTexture();

    // Vertical layout: compute the block height first so the composition is
    // centered on the display (not a small dialog stuck to the top), and
    // shrink the logo if needed so it always fits.
    const float gapAfterLogo = 24.0f * s;
    const float titleAdv = 52.0f * s;
    const float betaAdv = 32.0f * s;
    const float verAdv = version_.empty() ? 0.0f : 26.0f * s;
    const float preDividerGap = 6.0f * s;
    const float stripeH = 3.0f * s;
    const float afterDividerGap = 26.0f * s;
    const float barH = 16.0f * s;
    const float afterBarGap = 7.0f * s;
    const float statusAdv = 16.0f * s;

    const float fixed = gapAfterLogo + titleAdv + betaAdv + verAdv +
        preDividerGap + stripeH + afterDividerGap + barH + afterBarGap + statusAdv;

    float logoMax = SDL_min(contentW * 0.46f, H * 0.36f);
    const float maxTotal = SDL_max(200.0f, H - 2.0f * (H * 0.06f));
    if (logoMax + fixed > maxTotal)
        logoMax = SDL_max(72.0f, maxTotal - fixed);

    float y = SDL_max(H * 0.06f, (H - (logoMax + fixed)) * 0.5f);

    float logoH = logoMax;
    DrawLogoOrFallback(cx, y, logoMax, logoH);
    y += logoH + gapAfterLogo;

    // Release hierarchy.
    DrawCenteredText(renderer_, cx, y, 42.0f * s, productName_, true, kCream);
    y += titleAdv;

    DrawCenteredText(renderer_, cx, y, 21.0f * s, O2emVersion::kReleaseName,
                     true, kRed);
    y += betaAdv;

    if (!version_.empty())
    {
        DrawCenteredText(renderer_, cx, y, 14.0f * s, version_, false, kMuted);
        y += verAdv;
    }

    y += preDividerGap;
    DrawStripeDivider(renderer_, cx - contentW * 0.5f, y, contentW, stripeH);
    y += stripeH + afterDividerGap;

    // Progress.
    DrawProgressBar(cx - contentW * 0.5f, y, contentW, barH);

    // Status line.
    const float statusY = y + barH + afterBarGap;
    SetColor(renderer_, kMuted);
    UiFont_DrawText(renderer_, cx - contentW * 0.5f, statusY, 13.5f * s, stageLabel_);

    if (!finished_ && stageCount_ > 0)
    {
        char counter[32];
        std::snprintf(counter, sizeof(counter), "%d / %d",
            SDL_min(stageCompleted_, stageCount_), stageCount_);
        DrawRightText(renderer_, cx + contentW * 0.5f, statusY, 13.5f * s,
            counter, false, kMuted);
    }
}

void StartupSplash::DrawLogoOrFallback(float cx, float topY, float maxSize, float& outHeight)
{
    outHeight = maxSize;

    if (logo_)
    {
        float lw = 1.0f, lh = 1.0f;
        if (!SDL_GetTextureSize(logo_, &lw, &lh) || lw <= 0.0f || lh <= 0.0f)
        {
            DrawFallbackEmblem(cx, topY, maxSize);
            return;
        }
        const float scale = maxSize / SDL_max(lw, lh);
        const float dw = lw * scale;
        const float dh = lh * scale;
        SDL_FRect dst{ cx - dw * 0.5f, topY, dw, dh };
        SDL_RenderTexture(renderer_, logo_, nullptr, &dst);
        outHeight = dh;
        return;
    }

    DrawFallbackEmblem(cx, topY, maxSize);
}

void StartupSplash::DrawFallbackEmblem(float cx, float topY, float size)
{
    // Clean text-based fallback: cream ring + wordmark.
    const float cy = topY + size * 0.5f;
    const float r = size * 0.5f;
    const int segments = 72;

    SetColor(renderer_, kCream);
    for (int i = 0; i < segments; ++i)
    {
        const float a0 = (6.2831853f * i) / segments;
        const float a1 = (6.2831853f * (i + 1)) / segments;
        SDL_RenderLine(renderer_,
            cx + SDL_cosf(a0) * r, cy + SDL_sinf(a0) * r,
            cx + SDL_cosf(a1) * r, cy + SDL_sinf(a1) * r);
        SDL_RenderLine(renderer_,
            cx + SDL_cosf(a0) * (r - 2.0f), cy + SDL_sinf(a0) * (r - 2.0f),
            cx + SDL_cosf(a1) * (r - 2.0f), cy + SDL_sinf(a1) * (r - 2.0f));
    }

    DrawCenteredText(renderer_, cx, cy - size * 0.11f, size * 0.15f,
        productName_, true, kCream);
}

void StartupSplash::DrawProgressBar(float x, float y, float w, float h)
{
    const SDL_FRect track{ x, y, w, h };

    // One clean dark track with a subtle inset edge.
    Fill(renderer_, track, kTrack);
    SetColor(renderer_, kTrackEdge);
    SDL_RenderLine(renderer_, track.x, track.y, track.x + track.w - 1.0f, track.y);
    SDL_RenderLine(renderer_, track.x, track.y, track.x, track.y + track.h - 1.0f);
    SetColor(renderer_, kPanelLine);
    SDL_RenderLine(renderer_, track.x, track.y + track.h - 1.0f,
        track.x + track.w - 1.0f, track.y + track.h - 1.0f);
    SDL_RenderLine(renderer_, track.x + track.w - 1.0f, track.y,
        track.x + track.w - 1.0f, track.y + track.h - 1.0f);

    const SDL_FRect inner{ track.x + 2.0f, track.y + 2.0f,
        track.w - 4.0f, track.h - 4.0f };

    // One single blue fill that steps cleanly with the real startup stages.
    // No independent timer, no fake percentage: the width is exactly the real
    // stage fraction.
    const float frac = (stageCount_ > 0)
        ? static_cast<float>(stageCompleted_) / static_cast<float>(stageCount_)
        : 0.0f;
    const float fw = inner.w * SDL_clamp(frac, 0.0f, 1.0f);
    if (fw > 0.0f)
    {
        const SDL_FRect fill{ inner.x, inner.y, fw, inner.h };
        Fill(renderer_, fill, kFill);

        // Subtle top highlight only.
        const float hl = SDL_max(1.0f, inner.h * 0.18f);
        const SDL_FRect gloss{ inner.x + 1.0f, inner.y + 1.0f,
            SDL_max(0.0f, fw - 2.0f), hl };
        if (gloss.w > 0.0f)
        {
            SetColor(renderer_, kFillHi);
            SDL_RenderFillRect(renderer_, &gloss);
        }
    }
}

void StartupSplash::Present()
{
    if (renderer_)
        SDL_RenderPresent(renderer_);
}

void StartupSplash::PumpEvents()
{
    SDL_Event event;
    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
            quitRequested_ = true;
        else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && window_ &&
            event.window.windowID == SDL_GetWindowID(window_))
            quitRequested_ = true;
    }
}

void StartupSplash::PlayStartupSound(const std::string& wavPath)
{
    // One-shot boot logging: flush immediately so the result is visible even
    // if the process is terminated right after startup.
    const auto startupLog = [](const char* fmt, auto... args)
    {
        std::printf(fmt, args...);
        std::fflush(stdout);
    };

    if (soundAttempted_)
        return;
    soundAttempted_ = true;

    if (wavPath.empty())
        return;

    size_t fileLen = 0;
    Uint8* raw = static_cast<Uint8*>(SDL_LoadFile(wavPath.c_str(), &fileLen));
    if (!raw || fileLen < 44)
    {
        startupLog("O2EM-NG: startup sound not available (%s); continuing.\n",
            wavPath.c_str());
        if (raw)
            SDL_free(raw);
        return;
    }

    Uint16 audioFormat = 0, channels = 0, bits = 0;
    Uint32 sampleRate = 0;
    const Uint8* pcm = nullptr;
    Uint32 pcmLen = 0;

    if (std::memcmp(raw, "RIFF", 4) == 0 && std::memcmp(raw + 8, "WAVE", 4) == 0)
    {
        size_t pos = 12;
        while (pos + 8 <= fileLen)
        {
            const Uint8* chunk = raw + pos;
            const Uint32 chunkSize = ReadU32LE(chunk + 4);
            const size_t body = pos + 8;
            if (body + chunkSize > fileLen)
                break;

            if (std::memcmp(chunk, "fmt ", 4) == 0 && chunkSize >= 16)
            {
                audioFormat = ReadU16LE(raw + body);
                channels = ReadU16LE(raw + body + 2);
                sampleRate = ReadU32LE(raw + body + 4);
                bits = ReadU16LE(raw + body + 14);
            }
            else if (std::memcmp(chunk, "data", 4) == 0)
            {
                pcm = raw + body;
                pcmLen = chunkSize;
            }

            pos = body + chunkSize + (chunkSize & 1u);
        }
    }

    SDL_AudioFormat format = SDL_AUDIO_UNKNOWN;
    if (audioFormat == 1) // PCM
    {
        if (bits == 8) format = SDL_AUDIO_U8;
        else if (bits == 16) format = SDL_AUDIO_S16LE;
        else if (bits == 32) format = SDL_AUDIO_S32LE;
    }
    else if (audioFormat == 3 && bits == 32) // IEEE float
    {
        format = SDL_AUDIO_F32LE;
    }

    if (format == SDL_AUDIO_UNKNOWN || channels == 0 || sampleRate == 0 || !pcm || pcmLen == 0)
    {
        startupLog("O2EM-NG: startup sound has an unsupported WAV format (%s); continuing.\n",
            wavPath.c_str());
        SDL_free(raw);
        return;
    }

    SDL_AudioSpec spec{};
    spec.format = format;
    spec.channels = static_cast<int>(channels);
    spec.freq = static_cast<int>(sampleRate);

    audioStream_ = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);

    if (!audioStream_)
    {
        startupLog("O2EM-NG: startup sound could not open audio (%s); continuing.\n",
            SDL_GetError());
        SDL_free(raw);
        return;
    }

    if (!SDL_PutAudioStreamData(audioStream_, pcm, static_cast<int>(pcmLen)))
    {
        startupLog("O2EM-NG: startup sound could not queue audio (%s); continuing.\n",
            SDL_GetError());
        CloseAudio();
        SDL_free(raw);
        return;
    }

    SDL_FlushAudioStream(audioStream_);

    if (!SDL_ResumeAudioStreamDevice(audioStream_))
    {
        startupLog("O2EM-NG: startup sound could not resume audio (%s); continuing.\n",
            SDL_GetError());
        CloseAudio();
        SDL_free(raw);
        return;
    }

    const Uint32 bytesPerFrame = static_cast<Uint32>(channels) * (bits / 8u);
    const Uint32 frames = bytesPerFrame ? (pcmLen / bytesPerFrame) : 0;
    const Uint64 durationMs = sampleRate ? (1000ull * frames / sampleRate) : 0ull;
    audioStopAt_ = SDL_GetTicks() + durationMs + 250ull;

    startupLog("O2EM-NG: playing G7000 startup sound (%.2fs).\n",
        durationMs / 1000.0);
    SDL_free(raw);
}

void StartupSplash::CloseAudio()
{
    if (audioStream_)
    {
        SDL_DestroyAudioStream(audioStream_);
        audioStream_ = nullptr;
    }
}
