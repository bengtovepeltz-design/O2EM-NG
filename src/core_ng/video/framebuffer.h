#pragma once
// NG-0 framebuffer contract. Devices render into this; the app presents it.

#include <cstdint>
#include <vector>

namespace o2em::ng {

enum class PixelFormat : std::uint8_t { Indexed8, Rgb32 };

struct Framebuffer {
    int width = 0;
    int height = 0;
    int stride = 0;
    PixelFormat format = PixelFormat::Indexed8;
    std::vector<std::uint8_t> pixels;
    std::uint64_t frame_number = 0;

    void Resize(int w, int h, PixelFormat f) {
        width = w; height = h; format = f;
        stride = w * (f == PixelFormat::Rgb32 ? 4 : 1);
        pixels.assign(static_cast<std::size_t>(stride) * h, 0);
    }
};

} // namespace o2em::ng
