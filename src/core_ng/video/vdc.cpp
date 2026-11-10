#include "vdc.h"

#include <algorithm>
#include <cstring>

#include "character_rom.h"
#include "vpp.h"
#include "../audio/audio_device.h"

namespace o2em::ng {

const VdcRgb kI8244PaletteRgb[16] = {
    {0x00, 0x00, 0x00}, {0xB6, 0x00, 0x00}, {0x00, 0xB6, 0x00}, {0xB6, 0xB6, 0x00},
    {0x00, 0x00, 0xB6}, {0xB6, 0x00, 0xB6}, {0x00, 0xB6, 0xB6}, {0xB6, 0xB6, 0xB6},
    {0x49, 0x49, 0x49}, {0xFF, 0x49, 0x49}, {0x49, 0xFF, 0x49}, {0xFF, 0xFF, 0x49},
    {0x49, 0x49, 0xFF}, {0xFF, 0x49, 0xFF}, {0x49, 0xFF, 0xFF}, {0xFF, 0xFF, 0xFF},
};

namespace {

constexpr std::uint32_t kSnapshotMagic = 0x33434456u; // "VDC3"
constexpr std::uint32_t kSnapshotVersion = 1u;

inline bool InRange(int v, int lo, int hi) { return v >= lo && v < hi; }

// MAME util::bitswap semantics: the first source bit becomes the most
// significant destination bit.
inline std::uint8_t Bitswap3(std::uint8_t v, int b0, int b1, int b2) {
    return static_cast<std::uint8_t>(((v >> b0) & 1) << 2 | ((v >> b1) & 1) << 1 |
                                     ((v >> b2) & 1));
}
inline std::uint8_t Bitswap4(std::uint8_t v, int b0, int b1, int b2, int b3) {
    return static_cast<std::uint8_t>(((v >> b0) & 1) << 3 | ((v >> b1) & 1) << 2 |
                                     ((v >> b2) & 1) << 1 | ((v >> b3) & 1));
}

// First HBL start tick at or after `tick` on the global line grid.
Tick NextHblankTick(const HardwareProfile& p, Tick tick) {
    const Tick line_ticks = p.LineTicks();
    if (line_ticks <= 0) return tick;
    const Tick line = tick / line_ticks;
    Tick hbl = line * line_ticks + std::int64_t(p.hblank_start) * p.pixel_ticks;
    if (hbl < tick) hbl += line_ticks;
    return hbl;
}

} // namespace

void Vdc::Configure(const HardwareProfile& profile) {
    profile_ = profile;
    const int w = profile_.visible_w > 0 ? profile_.visible_w : kVdcVisibleWidth;
    const int h = profile_.visible_h > 0 ? profile_.visible_h : kVdcVisibleHeight;
    live_.Resize(w, h, PixelFormat::Indexed8);
    presented_.Resize(w, h, PixelFormat::Indexed8);
    Reset();
}

void Vdc::Reset() {
    std::memset(regs_, 0, sizeof(regs_));
    control_status_ = 0;
    collision_status_ = 0;
    x_latch_ = 0;
    y_latch_ = 0;
    irq_line_ = false;
    ClearCollisionMaps();
    ClearLiveFrame();
    if (!presented_.pixels.empty()) {
        std::memset(presented_.pixels.data(), 0, presented_.pixels.size());
    }
    presented_.frame_number = 0;
    completed_frames_ = 0;
    cursor_line_ = 0;
    cursor_x_ = 0;
    initialized_ = false;
    last_tick_ = 0;
    frame_index_ = 0;
    tick_ = 0;
    sh_written_ = false;
    sh_pending_ = false;
    sh_prescaler_ = 0;
    sh_count_ = 0;
    sound_output_ = 0;
    sh_duty_ = 0;
    next_hblank_tick_ = 0;
}

void Vdc::AdvanceTo(Tick tick) {
    if (tick < 0) tick = 0;
    const Tick frame_ticks = profile_.FrameTicks();
    if (frame_ticks <= 0) return;

    if (!initialized_) {
        frame_index_ = tick / frame_ticks;
        last_tick_ = tick;
        tick_ = tick;
        cursor_line_ = 0;
        cursor_x_ = 0;
        ClearLiveFrame();
        ClearCollisionMaps();
        next_hblank_tick_ = NextHblankTick(profile_, tick);
        initialized_ = true;
        // A reset in the middle of a frame starts a fresh visible frame from
        // the current beam; the pixels before this point are deliberately
        // blank (hardware reset clears the video output).
        return;
    }

    if (tick <= last_tick_) {
        tick_ = tick;
        return;
    }

    const std::int64_t target_frame = tick / frame_ticks;
    while (frame_index_ < target_frame) {
        RenderTo(profile_.visible_h, 0);
        FinalizeFrame();
        ++frame_index_;
        cursor_line_ = 0;
        cursor_x_ = 0;
        ClearLiveFrame();
        ClearCollisionMaps();
    }

    const Tick phase = tick % frame_ticks;
    const int vpos = static_cast<int>(phase / profile_.LineTicks());
    const int hpos =
        static_cast<int>((phase % profile_.LineTicks()) / profile_.pixel_ticks);
    const int target_line =
        vpos >= profile_.visible_h ? profile_.visible_h : vpos;
    const int target_x = hpos >= profile_.visible_w ? profile_.visible_w : hpos;
    RenderTo(target_line, target_x);

    while (next_hblank_tick_ <= tick) {
        ClockSound();
        next_hblank_tick_ += profile_.LineTicks();
    }
    if (audio_) audio_->AdvanceTo(tick);

    last_tick_ = tick;
    tick_ = tick;
}

void Vdc::ClearLiveFrame() {
    if (!live_.pixels.empty()) std::memset(live_.pixels.data(), 0, live_.pixels.size());
}

void Vdc::ClearCollisionMaps() {
    std::memset(collision_map_, 0, sizeof(collision_map_));
    std::memset(priority_map_, 0, sizeof(priority_map_));
}

void Vdc::FinalizeFrame() {
    presented_ = live_;
    presented_.frame_number = ++completed_frames_;
}

int Vdc::Hpos(Tick tick) const {
    if (tick < 0) tick = 0;
    const Tick line_ticks = profile_.LineTicks();
    if (line_ticks <= 0 || profile_.pixel_ticks <= 0) return 0;
    const Tick frame = profile_.FrameTicks();
    const Tick phase = frame > 0 ? tick % frame : tick;
    return static_cast<int>((phase % line_ticks) / profile_.pixel_ticks);
}

int Vdc::Vpos(Tick tick) const {
    if (tick < 0) tick = 0;
    const Tick line_ticks = profile_.LineTicks();
    if (line_ticks <= 0 || profile_.vtotal <= 0) return 0;
    const Tick frame = profile_.FrameTicks();
    const Tick phase = frame > 0 ? tick % frame : tick;
    return static_cast<int>((phase / line_ticks) % profile_.vtotal);
}

int Vdc::XBeam(Tick tick) const { return Hpos(tick) >> 1; }

int Vdc::YBeam(Tick tick) const {
    int v = Vpos(tick);
    const int h = Hpos(tick);
    if (v == 0 && h < profile_.hblank_start) v = profile_.vtotal;
    if (h >= profile_.bgate_start) ++v;
    return std::min(v, 263);
}

bool Vdc::Vblank() const {
    const int h = Hpos(tick_);
    const int v = Vpos(tick_);
    const int start = profile_.vblank_start;
    const int end = profile_.vblank_end;
    if ((v == start && h >= (profile_.hblank_start - 1)) ||
        (v == end && h <= (profile_.hblank_start - 1)))
        return true;
    if (end < start) return v > start || v < end;
    return v > start && v < end;
}

bool Vdc::Hblank() const {
    const int h = Hpos(tick_);
    const int start = profile_.hblank_start;
    const int end = profile_.hblank_end;
    if (end < start) return h >= start || h < end;
    return h >= start && h < end;
}

void Vdc::AssertVblankIrq() {
    control_status_ |= kStatusVblank;
    irq_line_ = true;
}

std::uint8_t Vdc::FixRegisterMirrors(std::uint8_t offset) {
    // Quad x/y registers are mirrored to the first character of each quad.
    if ((offset & 0xC2) == 0x40) offset &= static_cast<std::uint8_t>(~0x0C);
    // Registers A0-AF are mirrored at B0-BF.
    if ((offset & 0xE0) == 0xA0) offset &= static_cast<std::uint8_t>(~0x10);
    return offset;
}

std::uint8_t Vdc::Read(std::uint8_t raw) {
    AdvanceTo(tick_);
    const std::uint8_t offset = FixRegisterMirrors(raw);
    switch (offset) {
    case kRegStatus: {
        std::uint8_t data = control_status_;
        const int h = Hpos(tick_);
        if (h >= 225 && h < profile_.bgate_start &&
            YBeam(tick_) <= profile_.vblank_start)
            data |= 0x01;
        data |= static_cast<std::uint8_t>(regs_[kRegControl] & kControlStrobe);
        if (audio_ && audio_->IrqLine()) data |= kStatusSoundIrq;
        // Reading A1 acknowledges the VBL/sound/external/major IRQs and clears
        // the latched high status bits. hstatus and position-strobe are live.
        irq_line_ = false;
        if (audio_) audio_->ClearIrq();
        control_status_ &= static_cast<std::uint8_t>(~0xCC);
        return data;
    }
    case kRegCollision: {
        const std::uint8_t data = collision_status_;
        collision_status_ = 0;
        return data;
    }
    case kRegYBeam:
        return (regs_[kRegControl] & kControlStrobe)
                   ? static_cast<std::uint8_t>(YBeam(tick_))
                   : y_latch_;
    case kRegXBeam:
        return (regs_[kRegControl] & kControlStrobe)
                   ? static_cast<std::uint8_t>(XBeam(tick_))
                   : x_latch_;
    case 0x02:
    case 0x06:
    case 0x0A:
    case 0x0E:
    case kRegColor:
    case kRegShift1:
    case kRegShift2:
    case kRegShift3:
        return 0;
    default:
        break;
    }

    std::uint8_t data = regs_[offset];
    if (offset < 0x80 && (regs_[kRegControl] & kControlDisplay))
        data = 0;
    else if (offset >= 0xC0 && (regs_[kRegControl] & kControlGrid))
        data = 0;
    return data;
}

void Vdc::Write(std::uint8_t raw, std::uint8_t value) {
    AdvanceTo(tick_);
    const std::uint8_t offset = FixRegisterMirrors(raw);

    // Object x/y/attr registers are inaccessible while display is enabled;
    // sprite shape registers >= 0x80 remain accessible.
    if (offset < 0x80 && (regs_[kRegControl] & kControlDisplay)) return;    // Grid registers are inaccessible while grid is enabled; >= F0 unmapped.
    if ((offset >= 0xC0 && (regs_[kRegControl] & kControlGrid)) || offset >= 0xF0)
        return;

    // Color registers D4-D7 are not connected.
    if ((offset & 0x83) == 0x03) value &= 0x0F;
    // Major-system Y CAM d0 is not connected.
    if (offset >= 0x10 && (offset & 0x83) == 0x00) value &= static_cast<std::uint8_t>(~0x01);
    // Horizontal grid high byte only d0 is connected.
    if ((offset & 0xF0) == 0xD0) value &= 0x01;

    switch (offset) {
    case kRegControl:
        if ((regs_[kRegControl] & kControlStrobe) && !(value & kControlStrobe)) {
            x_latch_ = static_cast<std::uint8_t>(XBeam(tick_));
            y_latch_ = static_cast<std::uint8_t>(YBeam(tick_));
        }
        break;
    case kRegShift1:
    case kRegShift2:
    case kRegShift3:
        if (audio_) audio_->WriteRegister(offset, value, tick_);
        else sh_written_ = true;
        break;
    case kRegSound:
        value &= static_cast<std::uint8_t>(~0x40);
        if (audio_) audio_->WriteRegister(0xAA, value, tick_);
        break;
    case kRegStatus:
    case kRegYBeam:
    case kRegXBeam:
        return;
    case 0x03:
    case 0x07:
    case 0x0B:
    case 0x0F:
    case 0xA6:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xAE:
    case 0xAF:
    case 0xC9:
    case 0xCA:
    case 0xCB:
    case 0xCC:
    case 0xCD:
    case 0xCE:
    case 0xCF:
    case 0xD9:
    case 0xDA:
    case 0xDB:
    case 0xDC:
    case 0xDD:
    case 0xDE:
    case 0xDF:
    case 0xEA:
    case 0xEB:
    case 0xEC:
    case 0xED:
    case 0xEE:
    case 0xEF:
        return;
    default:
        break;
    }
    regs_[offset] = value;
}

std::uint8_t Vdc::BackgroundColor() const {
    return Bitswap3(regs_[kRegColor], 3, 4, 5);
}

std::uint8_t Vdc::GridColor() const {
    return Bitswap4(regs_[kRegColor], 6, 0, 1, 2);
}

std::uint8_t Vdc::MajorColor(std::uint8_t color) {
    return static_cast<std::uint8_t>(8 + ((color >> 1) & 0x07));
}

std::uint8_t Vdc::MinorColor(std::uint8_t color) {
    return static_cast<std::uint8_t>(8 + ((color >> 3) & 0x07));
}

void Vdc::RenderTo(int line, int x) {
    if (line < 0) line = 0;
    if (line > profile_.visible_h) line = profile_.visible_h;
    if (x < 0) x = 0;
    if (x > profile_.visible_w) x = profile_.visible_w;

    while (cursor_line_ < line && cursor_line_ < profile_.visible_h) {
        // MAME clears the collision and priority maps at the start of every
        // scanline (while rendering from the left edge). A mid-line partial
        // update keeps the maps for the already-rendered left part.
        if (cursor_x_ == 0) ClearCollisionMaps();
        if (cursor_x_ < profile_.visible_w)
            RenderSegment(cursor_line_, cursor_x_, profile_.visible_w);
        EdgePass(cursor_line_);
        if (vpp_) vpp_->CompositeLine(cursor_line_, this);
        ++cursor_line_;
        cursor_x_ = 0;
    }
    if (cursor_line_ < profile_.visible_h && line == cursor_line_ && x > cursor_x_) {
        if (cursor_x_ == 0) ClearCollisionMaps();
        RenderSegment(cursor_line_, cursor_x_, x);
        cursor_x_ = x;
    }
}

void Vdc::RenderSegment(int line, int x0, int x1) {
    if (line < 0 || line >= profile_.visible_h) return;
    if (x0 < 0) x0 = 0;
    if (x1 > profile_.visible_w) x1 = profile_.visible_w;
    if (x0 >= x1) return;

    const int stride = live_.stride;
    const std::uint8_t bg = BackgroundColor();
    for (int x = x0; x < x1; ++x)
        live_.pixels[static_cast<std::size_t>(line) * stride + x] = bg;

    if ((regs_[kRegControl] & kControlGrid) && line >= 24 && line <= 218)
        RenderGrid(line, x0, x1);
    if ((regs_[kRegControl] & kControlDisplay) && line <= 242) {
        RenderMajor(line, x0, x1);
        RenderMinor(line, x0, x1);
    }
}

void Vdc::RenderGrid(int line, int x0, int x1) {
    const std::uint8_t color = GridColor();
    const int stride = live_.stride;
    const int x_grid_offset = 13;
    const int y_grid_offset = 24;
    const int width = 16;
    const int height = 24;
    const int w = (regs_[kRegControl] & kControlGridWide) ? width : 2;

    auto plot = [&](int x, std::uint8_t collision_bit) {
        for (int px = x; px < x + 2; ++px) {
            if (px < x0 || px >= x1 || px < 0 || px >= profile_.visible_w) continue;
            collision_map_[px] |= collision_bit;
            live_.pixels[static_cast<std::size_t>(line) * stride + px] = color;
        }
    };

    // Horizontal grid segments.
    for (int gy = 0; gy < 9; ++gy) {
        const int y0 = y_grid_offset + gy * height;
        if (line < y0 || line >= y0 + 3) continue;
        for (int i = 0; i < 9; ++i) {
            const std::uint16_t bits = static_cast<std::uint16_t>(
                (regs_[kRegHGrid1 + i] << 8) | regs_[kRegHGrid0 + i]);
            if (!(bits & (1u << gy))) continue;
            for (int k = 0; k < width + 2; ++k)
                plot((x_grid_offset + i * width + k) * 2, kCollisionGridHorizontal);
        }
    }

    // Grid dots.
    if (regs_[kRegControl] & kControlGridDots) {
        for (int gy = 0; gy < 9; ++gy) {
            const int y0 = y_grid_offset + gy * height;
            if (line < y0 || line >= y0 + 3) continue;
            for (int i = 0; i < 10; ++i)
                for (int k = 0; k < 2; ++k)
                    plot((x_grid_offset + i * width + k) * 2,
                         kCollisionGridHorizontal);
        }
    }

    // Vertical grid segments.
    for (int gy = 0; gy < 8; ++gy) {
        const int y0 = y_grid_offset + gy * height;
        const int y1 = y0 + height;
        if (line < y0 || line >= y1) continue;
        const std::uint8_t bit = static_cast<std::uint8_t>(1u << gy);
        for (int i = 0; i < 10; ++i) {
            if (!(regs_[kRegVGrid + i] & bit)) continue;
            for (int k = 0; k < w; ++k)
                plot((x_grid_offset + i * width + k) * 2,
                     kCollisionGridVertical);
        }
    }
}

void Vdc::MajorPixel(std::uint8_t index, int x, int line, std::uint8_t pixel,
                     std::uint8_t color, int x0, int x1) {
    const int stride = live_.stride;
    const std::uint8_t mask = regs_[kRegCollision];
    for (int px = x; px < x + 2; ++px) {
        if (px < x0 || px >= x1 || px < 0 || px >= profile_.visible_w) continue;
        std::uint8_t colx = static_cast<std::uint8_t>(collision_map_[px]);
        if (index < priority_map_[px]) {
            control_status_ |= kStatusMajorCollision;
            // Hardware major/major priority overlap: keep the earlier pixel.
            if (colx & kCollisionMajor) continue;
        } else {
            priority_map_[px] = index;
        }
        if (pixel) {
            if (mask & colx) collision_status_ |= kCollisionMajor;
            if (mask & kCollisionMajor) collision_status_ |= colx;
            collision_map_[px] |= kCollisionMajor;
            live_.pixels[static_cast<std::size_t>(line) * stride + px] = color;
        }
    }
}

void Vdc::RenderMajor(int line, int x0, int x1) {
    // Quads: 4 quads x 4 characters. The quad's Y and X come from the first
    // character; its height is determined by the fourth character's pattern.
    for (int i = 3; i >= 0; --i) {
        const int base = kRegQuad + i * 0x10;
        const int y = regs_[base + 0];
        if (IsNtsc() && y < 0x0E) continue;
        const int ptr4 = regs_[base + 3 * 4 + 2];
        int height = 7 - (((y >> 1) + ptr4) & 7);
        if (height == 0) height = 8;
        if (line < y || line >= y + height * 2) continue;

        int x = (regs_[base + 1] + 5) * 2;
        for (int j = 0; j < 4; ++j) {
            const int cb = base + j * 4;
            const int ptr = regs_[cb + 2];
            const int col = regs_[cb + 3];
            const int offset =
                (ptr | ((col & 0x01) << 8)) + (y >> 1) + ((line - y) >> 1);
            const std::uint8_t color = MajorColor(static_cast<std::uint8_t>(col));
            for (int cx = 0; cx < 8; ++cx, x += 2) {
                const std::uint8_t bits =
                    kI8244CharacterRom[offset & 0x1FF];
                const std::uint8_t pixel =
                    static_cast<std::uint8_t>((bits >> (cx ^ 7)) & 1);
                MajorPixel(static_cast<std::uint8_t>(4 * j + 16 * i + 0x40), x,
                           line, pixel, color, x0, x1);
            }
            // MAME's quad loop advances x by 16 in the outer loop in addition
            // to the 16 already covered by the 8-column inner loop, giving
            // each quad character a 32-pixel pitch (16 drawn + 16 gap).
            x += 16;
        }
    }

    // Foreground major objects.
    for (int i = 11; i >= 0; --i) {
        const int base = kRegMajor + i * 4;
        const int y = regs_[base + 0];
        if (IsNtsc() && y < 0x0E) continue;
        const int ptr = regs_[base + 2];
        int height = 7 - (((y >> 1) + ptr) & 7);
        if (height == 0) height = 8;
        if (line < y || line >= y + height * 2) continue;

        const int col = regs_[base + 3];
        const int offset =
            (ptr | ((col & 0x01) << 8)) + (y >> 1) + ((line - y) >> 1);
        const std::uint8_t color = MajorColor(static_cast<std::uint8_t>(col));
        int x = (regs_[base + 1] + 5) * 2;
        for (int cx = 0; cx < 8; ++cx, x += 2) {
            const std::uint8_t bits = kI8244CharacterRom[offset & 0x1FF];
            const std::uint8_t pixel =
                static_cast<std::uint8_t>((bits >> (cx ^ 7)) & 1);
            MajorPixel(static_cast<std::uint8_t>(4 * i + 0x10), x, line, pixel,
                       color, x0, x1);
        }
    }
}

void Vdc::RenderMinor(int line, int x0, int x1) {
    const int stride = live_.stride;
    const std::uint8_t collision_mask = regs_[kRegCollision];
    for (int i = 3; i >= 0; --i) {
        const int base = kRegMinor + i * 4;
        const int y = regs_[base + 0];
        const int xb = regs_[base + 1];
        const int col = regs_[base + 2];
        const bool zoom = (col & 0x04) != 0;
        const int zoom_px = zoom ? 4 : 2;
        if (line < y || line >= y + 8 * zoom_px) continue;

        const std::uint8_t color = MinorColor(static_cast<std::uint8_t>(col));
        const int row = (line - y) / zoom_px;
        const std::uint8_t chr = regs_[kRegShape + i * 8 + row];
        int x = (xb + 5) * 2;
        int shift = 0;
        switch (col & 0x03) {
        case 1: shift = 1; break;                       // X9 attribute set
        case 2: shift = ((row & 0x01) ^ 0x01); break;   // S attribute set
        case 3: shift = (row & 0x01); break;            // X9 and S set
        default: break;
        }
        x += shift * (zoom_px / 2);

        for (std::uint8_t m = 0x01; m != 0; m = static_cast<std::uint8_t>(m << 1),
                          x += zoom_px) {
            if (!(chr & m)) continue;
            for (int px = x; px < x + zoom_px; ++px) {
                if (px < x0 || px >= x1 || px < 0 || px >= profile_.visible_w)
                    continue;
                const std::uint8_t mask8 = static_cast<std::uint8_t>(1u << i);
                const std::uint8_t colx = static_cast<std::uint8_t>(collision_map_[px]);
                if (collision_mask & colx) collision_status_ |= mask8;
                if (collision_mask & mask8) collision_status_ |= colx;
                collision_map_[px] |= mask8;
                if (zoom) collision_map_[px] |= 0x100;
                live_.pixels[static_cast<std::size_t>(line) * stride + px] = color;
            }
        }
    }
}

void Vdc::EdgePass(int line) {
    (void)line;
    const std::uint8_t mask = regs_[kRegCollision];
    for (int x = 1; x < profile_.visible_w; ++x) {
        const std::uint16_t c0 = collision_map_[x - 1];
        const std::uint16_t c1 = collision_map_[x];

        // Grid or minor to the left of a major object.
        if (c1 & kCollisionMajor) {
            if (mask & c0 & 0x3F) collision_status_ |= kCollisionMajor;
            if (mask & kCollisionMajor)
                collision_status_ |= static_cast<std::uint8_t>(c0 & 0x3F);
        }

        // Grid to the left of a non-zoomed minor object.
        const std::uint8_t m =
            static_cast<std::uint8_t>((c1 & 0x0F) & (~c1 >> 8));
        if (mask & c0 & 0x30) collision_status_ |= m;
        if (mask & m) collision_status_ |= static_cast<std::uint8_t>(c0 & 0x30);
    }
}

void Vdc::ExternalCollision(int x, bool asserted) {
    if (!asserted || x < 0 || x >= profile_.visible_w) return;
    const std::uint8_t colx = static_cast<std::uint8_t>(collision_map_[x]);
    if (colx) {
        if (regs_[kRegControl] & kControlExternalCollisionIrq) {
            irq_line_ = true;
            control_status_ |= kStatusExternalCollision;
        }
        if (colx & regs_[kRegCollision]) collision_status_ |= kCollisionExternal;
    }
    if (regs_[kRegCollision] & kCollisionExternal)
        collision_status_ |= colx;
}

std::uint8_t* Vdc::LiveRow(int line) {
    if (line < 0 || line >= profile_.visible_h || live_.pixels.empty())
        return nullptr;
    return live_.pixels.data() + static_cast<std::size_t>(line) * live_.stride;
}

bool Vdc::IrqLine() const {
    return irq_line_ || (audio_ && audio_->IrqLine());
}

std::uint8_t Vdc::Status() const {
    std::uint8_t status = control_status_;
    if (audio_ && audio_->IrqLine()) status |= kStatusSoundIrq;
    return status;
}

void Vdc::ClearIrq() {
    irq_line_ = false;
    if (audio_) audio_->ClearIrq();
}

bool Vdc::SoundBit() const {
    return audio_ ? audio_->ShiftOutput() : (sound_output_ != 0);
}

void Vdc::ClockSound() {
    if (audio_) {
        audio_->Hblank(next_hblank_tick_,
                       (regs_[kRegControl] & kControlSoundIrq) != 0);
        return;
    }
    // Standalone VDC fallback used by NG-3 device tests. The Machine always
    // attaches an AudioDevice, so this path never runs in production.
    sh_prescaler_++;
    const std::uint8_t prescaler_mask = (regs_[kRegSound] & 0x20) ? 3 : 0x0F;
    if ((sh_prescaler_ & prescaler_mask) == 0) sh_pending_ = true;

    if (sh_pending_ && !sh_written_) {
        sh_pending_ = false;
        std::uint32_t signal = regs_[kRegShift3] |
                               (static_cast<std::uint32_t>(regs_[kRegShift2]) << 8) |
                               (static_cast<std::uint32_t>(regs_[kRegShift1]) << 16);
        sound_output_ = static_cast<int>(signal & 1u);
        int feedback = sound_output_;
        signal >>= 1;
        if (regs_[kRegSound] & 0x10) {
            feedback ^= static_cast<int>((signal >> 4) & 1u);
            signal = (signal & ~0x8000u) | (static_cast<std::uint32_t>(feedback) << 15);
        }
        signal |= static_cast<std::uint32_t>(feedback) << 23;
        regs_[kRegShift3] = static_cast<std::uint8_t>(signal & 0xFF);
        regs_[kRegShift2] = static_cast<std::uint8_t>((signal >> 8) & 0xFF);
        regs_[kRegShift1] = static_cast<std::uint8_t>((signal >> 16) & 0xFF);

        if (++sh_count_ == 24) {
            sh_count_ = 0;
            if (regs_[kRegControl] & kControlSoundIrq) {
                control_status_ |= kStatusSoundIrq;
                irq_line_ = true;
            }
        }
    } else if (sh_written_) {
        sh_count_ = 0;
        sh_written_ = false;
    }
}

bool Vdc::SaveState(SnapshotWriter& w) const {
    w.u32(kSnapshotMagic);
    w.u32(kSnapshotVersion);
    w.u32(static_cast<std::uint32_t>(profile_.htotal));
    w.u32(static_cast<std::uint32_t>(profile_.vtotal));
    w.u32(static_cast<std::uint32_t>(profile_.visible_w));
    w.u32(static_cast<std::uint32_t>(profile_.visible_h));
    w.u32(static_cast<std::uint32_t>(profile_.pixel_ticks));
    w.u8(static_cast<std::uint8_t>(profile_.region));

    w.u64(static_cast<std::uint64_t>(tick_));
    w.u64(static_cast<std::uint64_t>(last_tick_));
    w.u64(static_cast<std::uint64_t>(frame_index_));
    w.u8(initialized_ ? 1 : 0);
    w.u32(static_cast<std::uint32_t>(cursor_line_));
    w.u32(static_cast<std::uint32_t>(cursor_x_));
    w.raw(regs_, sizeof(regs_));
    w.u8(control_status_);
    w.u8(collision_status_);
    w.u8(x_latch_);
    w.u8(y_latch_);
    w.u8(irq_line_ ? 1 : 0);

    for (std::uint16_t v : collision_map_) w.u16(v);
    w.raw(priority_map_, sizeof(priority_map_));

    w.u8(sh_written_ ? 1 : 0);
    w.u8(sh_pending_ ? 1 : 0);
    w.u8(sh_prescaler_);
    w.u8(sh_count_);
    w.u32(static_cast<std::uint32_t>(sound_output_));
    w.u8(sh_duty_);
    w.u64(static_cast<std::uint64_t>(next_hblank_tick_));
    w.u64(completed_frames_);

    w.u32(static_cast<std::uint32_t>(presented_.width));
    w.u32(static_cast<std::uint32_t>(presented_.height));
    w.u32(static_cast<std::uint32_t>(presented_.stride));
    w.u64(presented_.frame_number);
    w.u32(static_cast<std::uint32_t>(presented_.pixels.size()));
    if (!presented_.pixels.empty())
        w.raw(presented_.pixels.data(), presented_.pixels.size());

    w.u32(static_cast<std::uint32_t>(live_.pixels.size()));
    if (!live_.pixels.empty()) w.raw(live_.pixels.data(), live_.pixels.size());
    return true;
}

bool Vdc::LoadState(SnapshotReader& r) {
    if (r.u32() != kSnapshotMagic) return false;
    if (r.u32() != kSnapshotVersion) return false;
    const std::uint32_t htotal = r.u32();
    const std::uint32_t vtotal = r.u32();
    const std::uint32_t visible_w = r.u32();
    const std::uint32_t visible_h = r.u32();
    const std::uint32_t pixel_ticks = r.u32();
    const Region region = static_cast<Region>(r.u8());
    if (htotal != static_cast<std::uint32_t>(profile_.htotal) ||
        vtotal != static_cast<std::uint32_t>(profile_.vtotal) ||
        visible_w != static_cast<std::uint32_t>(profile_.visible_w) ||
        visible_h != static_cast<std::uint32_t>(profile_.visible_h) ||
        pixel_ticks != static_cast<std::uint32_t>(profile_.pixel_ticks) ||
        region != profile_.region)
        return false;

    tick_ = static_cast<Tick>(r.u64());
    last_tick_ = static_cast<Tick>(r.u64());
    frame_index_ = static_cast<std::int64_t>(r.u64());
    initialized_ = r.u8() != 0;
    cursor_line_ = static_cast<int>(r.u32());
    cursor_x_ = static_cast<int>(r.u32());
    r.raw(regs_, sizeof(regs_));
    control_status_ = r.u8();
    collision_status_ = r.u8();
    x_latch_ = r.u8();
    y_latch_ = r.u8();
    irq_line_ = r.u8() != 0;

    for (std::uint16_t& v : collision_map_) v = r.u16();
    r.raw(priority_map_, sizeof(priority_map_));

    sh_written_ = r.u8() != 0;
    sh_pending_ = r.u8() != 0;
    sh_prescaler_ = r.u8();
    sh_count_ = r.u8();
    sound_output_ = static_cast<int>(r.u32());
    sh_duty_ = r.u8();
    next_hblank_tick_ = static_cast<Tick>(r.u64());
    completed_frames_ = r.u64();

    const std::uint32_t pw = r.u32();
    const std::uint32_t ph = r.u32();
    const std::uint32_t ps = r.u32();
    presented_.frame_number = r.u64();
    const std::uint32_t psize = r.u32();
    if (pw != static_cast<std::uint32_t>(presented_.width) ||
        ph != static_cast<std::uint32_t>(presented_.height) ||
        ps != static_cast<std::uint32_t>(presented_.stride) ||
        psize != presented_.pixels.size())
        return false;
    if (psize) r.raw(presented_.pixels.data(), psize);

    const std::uint32_t lsize = r.u32();
    if (lsize != live_.pixels.size()) return false;
    if (lsize) r.raw(live_.pixels.data(), lsize);
    return r.ok;
}

} // namespace o2em::ng
