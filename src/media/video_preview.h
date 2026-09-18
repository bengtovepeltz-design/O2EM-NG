#pragma once
#include <SDL3/SDL.h>
#include <filesystem>
#include <memory>

// Silent, real-time looping preview. Decoding stays off the UI thread.
class VideoPreview
{
public:
    explicit VideoPreview(const std::filesystem::path& path);
    ~VideoPreview();
    bool Update(SDL_Renderer* renderer, SDL_Texture*& texture);
    bool Failed() const;
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
