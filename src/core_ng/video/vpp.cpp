#include "vpp.h"

#include <algorithm>
#include <cstring>

#include "ef9340_charset.h"

namespace o2em::ng {

namespace {
constexpr int kOffsetX = 15;
constexpr int kOffsetY = 5;
constexpr int kCols = 40;
constexpr int kVisibleH = 243;

inline std::uint8_t ReverseBits8(std::uint8_t value) {
    value = static_cast<std::uint8_t>(((value & 0xF0) >> 4) |
                                      ((value & 0x0F) << 4));
    value = static_cast<std::uint8_t>(((value & 0xCC) >> 2) |
                                      ((value & 0x33) << 2));
    return static_cast<std::uint8_t>(((value & 0xAA) >> 1) |
                                     ((value & 0x55) << 1));
}

inline std::uint16_t ExpandWidth(std::uint8_t c) {
    std::uint16_t r = 0;
    for (int i = 0; i < 8; ++i) {
        const std::uint16_t b = static_cast<std::uint16_t>((c >> i) & 1);
        r = static_cast<std::uint16_t>(r | (b << (2 * i)) | (b << (2 * i + 1)));
    }
    return r;
}

constexpr std::uint32_t kSnapshotMagic = 0x50505633u; // "VPP3"
constexpr std::uint32_t kSnapshotVersion = 1u;
} // namespace

void Vpp::Configure(const HardwareProfile& profile) {
    profile_ = profile;
    const int w = profile_.visible_w > 0 ? profile_.visible_w : 360;
    const int h = profile_.visible_h > 0 ? profile_.visible_h : kVisibleH;
    layer_.Resize(w, h, PixelFormat::Indexed8);
    composite_.Resize(w, h, PixelFormat::Indexed8);
    Reset();
}

void Vpp::Reset() {
    ta_ = tb_ = 0;
    busy_ = false;
    x_ = y_ = y0_ = r_ = m_ = 0;
    blink_ = false;
    blink_prescaler_ = 0;
    h_parity_ = false;
    mix_i8244_ = 0xFF;
    mix_ef934x_ = 0xFF;
    b_pin_ = false;
    tick_ = last_tick_ = 0;
    frame_index_ = 0;
    initialized_ = false;
    cursor_line_ = 0;
    completed_frames_ = 0;
    std::memset(ram_a_, 0, sizeof(ram_a_));
    std::memset(ram_b_, 0, sizeof(ram_b_));
    std::memset(extram_, 0, sizeof(extram_));
    ClearLive();
    if (!composite_.pixels.empty())
        std::memset(composite_.pixels.data(), 0, composite_.pixels.size());
    composite_.frame_number = 0;
}

void Vpp::ClearLive() {
    if (!layer_.pixels.empty())
        std::memset(layer_.pixels.data(), 0, layer_.pixels.size());
}

void Vpp::AdvanceTo(Tick tick) {
    if (tick < 0) tick = 0;
    tick_ = tick;
}

void Vpp::FrameTick() {
    // MAME's blink timer: one update per frame, 32-frame prescaler.
    blink_prescaler_ = (blink_prescaler_ + 1) & 0x1f;
    if ((r_ & 0x40) && blink_prescaler_ == 24) blink_prescaler_ = 0;
    if (blink_prescaler_ == 0) blink_ = !blink_;
    ++completed_frames_;
}

std::uint8_t Vpp::Read(std::uint8_t address) {
    if (!(address & 4)) return 0xFF;
    return Ef9341Read((address & 0x02) != 0, (address & 0x01) != 0);
}

void Vpp::Write(std::uint8_t address, std::uint8_t value) {
    if (address & 4) return;
    Ef9341Write((address & 0x02) != 0, (address & 0x01) != 0, value);
}

void Vpp::ExpanderWrite(int port, std::uint8_t value) {
    std::uint8_t mask = 0x0F;
    std::uint8_t data = value;
    if ((port & 1) == 0) { // P4/P6 -> high nibble
        data = static_cast<std::uint8_t>(value << 4);
        mask = 0xF0;
    }
    if (port >= 6)
        mix_i8244_ = static_cast<std::uint8_t>((mix_i8244_ & ~mask) | (data & mask));
    else
        mix_ef934x_ = static_cast<std::uint8_t>((mix_ef934x_ & ~mask) | (data & mask));
}

std::uint16_t Vpp::CAddr(int x, int y) {
    if ((y & 0x18) == 0x18)
        return static_cast<std::uint16_t>(0x318 | ((x & 0x38) << 2) | (x & 0x07));
    if (x & 0x20)
        return static_cast<std::uint16_t>(0x300 | ((y & 0x07) << 5) |
                                          (y & 0x18) | (x & 0x07));
    return static_cast<std::uint16_t>(((y & 0x1F) << 5) | (x & 0x1F));
}

void Vpp::IncC() {
    x_ = static_cast<std::uint8_t>(x_ + 1);
    if (x_ == 40 || x_ == 48 || x_ == 56 || x_ == 64) {
        y_ = static_cast<std::uint8_t>((y_ + 1) & 0x1F);
        if (y_ == 24) y_ = 0;
        x_ = 0;
    }
}

std::uint16_t Vpp::ExtRamAddress(std::uint32_t offset) const {
    const std::uint8_t latch = static_cast<std::uint8_t>(
        ((offset >> 12) & 0x80) | ((offset >> 4) & 0x7F));
    std::uint16_t address = static_cast<std::uint16_t>(
        (latch & 0x1F) | ((offset << 9) & 0x200) | ((latch << 3) & 0x400));
    if (offset & 8)
        address = static_cast<std::uint16_t>(address | (latch & 0x60));
    else
        address = static_cast<std::uint16_t>(
            address | ((offset << 4) & 0x60) | ((latch << 2) & 0x180));
    return static_cast<std::uint16_t>(address & 0x7FF);
}

std::uint8_t Vpp::ExtRamRead(std::uint32_t offset) const {
    return extram_[ExtRamAddress(offset)];
}

void Vpp::ExtRamWrite(std::uint32_t offset, std::uint8_t value) {
    extram_[ExtRamAddress(offset)] = value;
}

void Vpp::Ef9341Write(bool command, bool b, std::uint8_t data) {
    if (command) {
        if (b) {
            tb_ = data;
            busy_ = true;
            switch (tb_ & 0xE0) {
            case 0x00: x_ = 0; y_ = static_cast<std::uint8_t>(ta_ & 0x1F); break;
            case 0x20: y_ = static_cast<std::uint8_t>(ta_ & 0x1F); break;
            case 0x40: x_ = static_cast<std::uint8_t>(ta_ & 0x3F); break;
            case 0x60: IncC(); break;
            case 0x80: m_ = ta_; break;
            case 0xA0: r_ = ta_; break;
            case 0xC0: y0_ = static_cast<std::uint8_t>(ta_ & 0x3F); break;
            default: break;
            }
            busy_ = false;
        } else {
            ta_ = data;
        }
        return;
    }

    if (b) {
        const std::uint16_t addr = static_cast<std::uint16_t>(CAddr(x_, y_) & 0x3FF);
        tb_ = data;
        busy_ = true;
        switch (m_ & 0xE0) {
        case 0x00:
            ram_a_[addr] = ta_;
            ram_b_[addr] = tb_;
            IncC();
            break;
        case 0x40:
            ram_a_[addr] = ta_;
            ram_b_[addr] = tb_;
            break;
        case 0x80: {
            const std::uint8_t a = ram_a_[addr];
            const std::uint8_t ch = ram_b_[addr];
            const std::uint8_t slice = static_cast<std::uint8_t>(m_ & 0x0F);
            if (ch >= 0xA0)
                ExtRamWrite(static_cast<std::uint32_t>(a) << 12 |
                                static_cast<std::uint32_t>(ch) << 4 | slice,
                            ta_);
            m_ = static_cast<std::uint8_t>((m_ & 0xF0) | ((slice + 1) % 10));
            break;
        }
        default:
            break;
        }
        busy_ = false;
    } else {
        ta_ = data;
    }
}

std::uint8_t Vpp::Ef9341Read(bool command, bool b) {
    if (command) {
        if (b) return 0;
        return busy_ ? 0x80 : 0;
    }
    if (!b) return ta_;

    const std::uint16_t addr = static_cast<std::uint16_t>(CAddr(x_, y_) & 0x3FF);
    std::uint8_t data = tb_;
    busy_ = true;
    switch (m_ & 0xE0) {
    case 0x20:
        ta_ = ram_a_[addr];
        tb_ = ram_b_[addr];
        IncC();
        break;
    case 0x60:
        ta_ = ram_a_[addr];
        tb_ = ram_b_[addr];
        break;
    case 0xA0: {
        const std::uint8_t a = ram_a_[addr];
        const std::uint8_t ch = ram_b_[addr];
        const std::uint8_t slice = static_cast<std::uint8_t>(m_ & 0x0F);
        ta_ = 0xFF;
        tb_ = 0xFF;
        if (ch >= 0xA0)
            ta_ = ExtRamRead(static_cast<std::uint32_t>(a) << 12 |
                             static_cast<std::uint32_t>(ch) << 4 | slice);
        else if (ch < 0x80 && slice < 10)
            ta_ = kEf9340Charset[(((a & 0x80) | (ch & 0x7F)) * 10) + slice];
        m_ = static_cast<std::uint8_t>((m_ & 0xF0) | ((slice + 1) % 10));
        break;
    }
    default:
        break;
    }
    busy_ = false;
    return data;
}

void Vpp::DrawLine(int screen_line) {
    if (screen_line < 0 || screen_line >= profile_.visible_h) return;
    const int stride = layer_.stride;
    std::uint8_t* row = &layer_.pixels[static_cast<std::size_t>(screen_line) * stride];
    // Default: transparent layer row.
    std::memset(row, 0, static_cast<std::size_t>(stride));

    int vpos = screen_line - kOffsetY;
    if (vpos < 0) return;

    const int zoom = ((y0_ & 0x20) && vpos >= 10) ? 2 : 1;
    int slice = vpos % 10;
    if (zoom == 2) slice = ((vpos - 10) % 20) / 2;

    bool dh = false;
    if (vpos == 0) h_parity_ = false;

    const int max_vpos = (r_ & 0x40) ? 250 : 210;
    if (!(r_ & 0x01) || vpos >= max_vpos) return;

    int y_row = 0;
    std::uint16_t char_data = 0;
    std::uint8_t fg = 0;
    std::uint8_t bg = 0;
    bool conceal = false;
    bool box = (r_ & 0x02) != 0;
    bool underline = false;
    bool w_parity = false;

    if (vpos < 10) {
        if (r_ & 0x08) {
            y_row = 31;
        } else {
            return; // service row disabled: transparent
        }
    } else {
        y_row = ((y0_ & 0x1F) + (vpos - 10) / (10 * zoom)) % 24;
    }

    for (int x = 0; x < kCols; ++x) {
        int s = slice;
        const std::uint16_t addr = CAddr(x, y_row);
        const std::uint8_t a = ram_a_[addr];
        const std::uint8_t b = ram_b_[addr];
        bool blink = (r_ & 0x80) && blink_;
        const bool cursor = (r_ & 0x10) && x == x_ && y_row == y_;
        bool invert = cursor && !blink;
        bool dw = false;
        bool del = false;

        if (a & 0x80) {
            if ((b & 0xE0) != 0x80) {
                fg = static_cast<std::uint8_t>(a & 0x07);
                bg = static_cast<std::uint8_t>((a >> 4) & 0x07);
                if (b & 0x80)
                    char_data = ExtRamRead(static_cast<std::uint32_t>(a) << 12 |
                                           static_cast<std::uint32_t>(b) << 4 | s);
                else
                    char_data = ReverseBits8(kEf9340Charset[((b | 0x80) * 10) + s]);
            } else {
                char_data = 0xFF;
            }
        } else {
            if ((b & 0xE0) != 0x80) {
                if (a & 0x10) {
                    dh = true;
                    if (h_parity_) s += 10;
                    if (s > 0) s = (s - 1) / 2;
                }
                fg = static_cast<std::uint8_t>(a & 0x07);
                std::uint16_t c = 0;
                if (b & 0x80)
                    c = ExtRamRead(static_cast<std::uint32_t>(a) << 12 |
                                   static_cast<std::uint32_t>(b) << 4 | s);
                else if (s == 9 && underline)
                    c = 0xFF;
                else
                    c = ReverseBits8(kEf9340Charset[((b & 0x7F) * 10) + s]);

                dw = (a & 0x20) != 0;
                if (dw) {
                    if (!w_parity) char_data = ExpandWidth(static_cast<std::uint8_t>(c));
                } else {
                    char_data = c;
                }

                if (a & 0x40) {
                    invert = !invert;
                    blink = (r_ & 0x80) && !blink_;
                }
            } else {
                fg = static_cast<std::uint8_t>(a & 0x07);
                bg = static_cast<std::uint8_t>((a >> 4) & 0x07);
                char_data = 0xFF;
                del = true;
            }
        }

        if (blink && !cursor && (b & 0xE0) != 0x80 && (~a & 0x08))
            char_data = 0;
        if (invert) char_data ^= 0xFF;
        if (dw) w_parity = !w_parity; else w_parity = false;

        const int x0 = kOffsetX + x * 8;
        for (int i = 0; i < 8; ++i) {
            const std::uint8_t d = (conceal || box)
                                       ? 0
                                       : ((char_data & 1)
                                              ? static_cast<std::uint8_t>(fg & 7)
                                              : static_cast<std::uint8_t>(bg & 7));
            const int px = x0 + i;
            if (px >= 0 && px < stride)
                row[px] = static_cast<std::uint8_t>(d | (box ? 0 : 8));
            char_data >>= 1;
        }

        if (del) {
            conceal = (b & 0x01) && (r_ & 0x04);
            box = !(b & 0x02) && (r_ & 0x02);
            underline = (b & 0x04) != 0;
        }
    }

    if (vpos >= 10 && (vpos & 1) && slice == 9) {
        if (dh) h_parity_ = !h_parity_;
        else h_parity_ = false;
    }
}

void Vpp::CompositeLine(int screen_line, Vdc* vdc) {
    if (!vdc || screen_line < 0 || screen_line >= profile_.visible_h) return;
    if (screen_line == 0) FrameTick();
    DrawLine(screen_line);

    std::uint8_t* out = vdc->LiveRow(screen_line);
    if (!out) return;
    const std::uint8_t* erow =
        &layer_.pixels[static_cast<std::size_t>(screen_line) * layer_.stride];
    const std::uint8_t lum = static_cast<std::uint8_t>(((~p1_) >> 4) & 0x08);
    const int w = profile_.visible_w;

    for (int x = 0; x < w; ++x) {
        const std::uint16_t d = static_cast<std::uint16_t>(out[x] & 7);
        const std::uint16_t e_raw = erow[x];
        const bool en = (e_raw & 8) || !b_pin_;
        const std::uint16_t e = static_cast<std::uint16_t>(e_raw & 7);
        const bool i2 = !((mix_ef934x_ >> e) & 1);
        vdc->ExternalCollision(x, i2);
        if (en && ((mix_i8244_ >> d) & 1))
            out[x] = static_cast<std::uint8_t>(out[x] | lum);
        else
            out[x] = static_cast<std::uint8_t>(e | (i2 ? 8 : 0));
    }
}

void Vpp::Compose(const Framebuffer& vdc, std::uint8_t p1, Framebuffer& out,
                  Vdc* vdc_device) {
    (void)vdc;
    (void)p1;
    (void)out;
    (void)vdc_device;
    // Composition is performed per scanline in CompositeLine(), directly into
    // the VDC live frame, so the VDC's presented frame is the composite.
}

void Vpp::FinalizeFrame() {
    ++completed_frames_;
}

bool Vpp::SaveState(SnapshotWriter& w) const {
    w.u32(kSnapshotMagic);
    w.u32(kSnapshotVersion);
    w.u32(static_cast<std::uint32_t>(profile_.visible_w));
    w.u32(static_cast<std::uint32_t>(profile_.visible_h));
    w.u64(static_cast<std::uint64_t>(tick_));
    w.u8(b_pin_ ? 1 : 0);
    w.u8(p1_);
    w.u8(mix_i8244_);
    w.u8(mix_ef934x_);
    w.u8(ta_);
    w.u8(tb_);
    w.u8(busy_ ? 1 : 0);
    w.u8(x_);
    w.u8(y_);
    w.u8(y0_);
    w.u8(r_);
    w.u8(m_);
    w.u8(blink_ ? 1 : 0);
    w.u8(static_cast<std::uint8_t>(blink_prescaler_));
    w.u8(h_parity_ ? 1 : 0);
    w.raw(ram_a_, sizeof(ram_a_));
    w.raw(ram_b_, sizeof(ram_b_));
    w.raw(extram_, sizeof(extram_));
    w.u64(completed_frames_);
    w.u32(static_cast<std::uint32_t>(layer_.pixels.size()));
    if (!layer_.pixels.empty()) w.raw(layer_.pixels.data(), layer_.pixels.size());
    return true;
}

bool Vpp::LoadState(SnapshotReader& r) {
    if (r.u32() != kSnapshotMagic) return false;
    if (r.u32() != kSnapshotVersion) return false;
    if (r.u32() != static_cast<std::uint32_t>(profile_.visible_w) ||
        r.u32() != static_cast<std::uint32_t>(profile_.visible_h))
        return false;
    tick_ = static_cast<Tick>(r.u64());
    b_pin_ = r.u8() != 0;
    p1_ = r.u8();
    mix_i8244_ = r.u8();
    mix_ef934x_ = r.u8();
    ta_ = r.u8();
    tb_ = r.u8();
    busy_ = r.u8() != 0;
    x_ = r.u8();
    y_ = r.u8();
    y0_ = r.u8();
    r_ = r.u8();
    m_ = r.u8();
    blink_ = r.u8() != 0;
    blink_prescaler_ = r.u8();
    h_parity_ = r.u8() != 0;
    r.raw(ram_a_, sizeof(ram_a_));
    r.raw(ram_b_, sizeof(ram_b_));
    r.raw(extram_, sizeof(extram_));
    completed_frames_ = r.u64();
    const std::uint32_t layer_size = r.u32();
    if (layer_size != layer_.pixels.size()) return false;
    if (layer_size) r.raw(layer_.pixels.data(), layer_size);
    return r.ok;
}

} // namespace o2em::ng
