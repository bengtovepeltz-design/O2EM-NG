#pragma once

// ============================================================================
// src/frontend/startup_splash.h
//
// Isolated, removable startup/loading screen plus the one-shot application
// startup jingle.
//
// It draws into the existing main SDL window (the same renderer the frontend
// uses), so no extra window, artwork or thread is required. The window is
// created hidden by main.cpp; StartupSplash is shown only once it has a
// painted first frame, then the normal frontend paints over it when
// initialization completes.
//
// The primary visual is the round O2EM-NG badge (already shipped as
// assets/O2EM-NG_Icon.png). A clean text-based emblem is drawn if it cannot be
// loaded, so startup never depends on the artwork.
//
// This component is deliberately self-contained: removing the two files and
// the handful of call sites in main.cpp / FrontendApp::Initialize restores the
// previous startup behaviour.
// ============================================================================

#include <SDL3/SDL.h>

#include <string>

class StartupSplash
{
public:
    StartupSplash(SDL_Window* window, const char* productName, const char* version);
    ~StartupSplash();

    StartupSplash(const StartupSplash&) = delete;
    StartupSplash& operator=(const StartupSplash&) = delete;

    // Round logo to use as the primary visual. If it fails to load a clean
    // text emblem is used instead. Safe to call before the first Stage().
    void SetLogoPath(const std::string& path);

    // Number of real initialization stages the caller will report. Only used
    // to render a stage-based bar; no synthetic percentage is ever invented.
    void SetStageCount(int count);

    // Report a real startup stage (completed = number finished) and repaint.
    // Pumps the SDL event queue so the window stays responsive between the
    // synchronous initialization steps.
    void Stage(const char* label, int completed);

    // Paint the first frame, reveal the window with a short soft fade-in, then
    // leave the splash on screen. Call after the first Stage().
    void Show();

    // Paint the final "Ready" state, hold a short anti-flash minimum, then arm
    // the one-shot startup sound exactly once. Missing/corrupt WAV is logged
    // and ignored; startup continues.
    void FinishAndPlayStartupSound(const std::string& wavPath);

    // Cheap per-frame tick from the main loop: releases the startup audio
    // stream once the jingle has finished. No-op when no sound was played.
    void Tick();

    // Called before presenting a fatal error so the splash stops animating.
    void Abort();

    bool QuitRequested() const noexcept { return quitRequested_; }

private:
    void Draw();
    void Present();
    void PumpEvents();
    void DrawLogoOrFallback(float cx, float topY, float maxSize, float& outHeight);
    void DrawProgressBar(float x, float y, float w, float h);
    void DrawFallbackEmblem(float cx, float topY, float size);
    void FadeIn();
    void PlayStartupSound(const std::string& wavPath);
    void EnsureLogoTexture();
    void CloseAudio();

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* logo_ = nullptr;
    bool logoAttempted_ = false;
    std::string logoPath_;

    std::string productName_;
    std::string version_;
    std::string stageLabel_ = "Starting...";
    int stageCount_ = 0;
    int stageCompleted_ = 0;

    bool finished_ = false;
    bool aborted_ = false;
    bool quitRequested_ = false;
    Uint64 startTicks_ = 0;
    Uint64 minDisplayMs_ = 350;

    // One-shot application startup audio (does not touch the emulator audio
    // pipeline; a separate stream on the default playback device).
    SDL_AudioStream* audioStream_ = nullptr;
    Uint64 audioStopAt_ = 0;
    bool soundAttempted_ = false;
};
