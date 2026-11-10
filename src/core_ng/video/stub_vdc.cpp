#include "stub_vdc.h"

#include <algorithm>
#include <cstring>

namespace o2em::ng {

void StubVdc::Configure(const HardwareProfile& profile) {
    profile_ = profile;
    Reset();
}

void StubVdc::Reset() {
    std::memset(regs_, 0, sizeof(regs_));
    control_status_ = 0;
    collision_status_ = 0;
    x_latch_ = 0;
    y_latch_ = 0;
    irq_line_ = false;
    tick_ = 0;
}

void StubVdc::AssertVblankIrq() {
    control_status_ = static_cast<std::uint8_t>(control_status_ | 0x08);
    irq_line_ = true;
}

void StubVdc::ClearIrq() {
    irq_line_ = false;
}

int StubVdc::Hpos(Tick tick) const {
    if (tick < 0) tick = 0;
    const std::int64_t frame = profile_.FrameTicks();
    if (frame <= 0) return 0;
    const std::int64_t phase = tick % frame;
    return static_cast<int>((phase % profile_.LineTicks()) / profile_.pixel_ticks);
}

int StubVdc::Vpos(Tick tick) const {
    if (tick < 0) tick = 0;
    const std::int64_t frame = profile_.FrameTicks();
    if (frame <= 0) return 0;
    const std::int64_t phase = tick % frame;
    return static_cast<int>((phase / profile_.LineTicks()) % profile_.vtotal);
}

int StubVdc::XBeam(Tick tick) const {
    return Hpos(tick) >> 1;
}

int StubVdc::YBeam(Tick tick) const {
    int v = Vpos(tick);
    const int h = Hpos(tick);
    if (v == 0 && h < profile_.hblank_start) v = profile_.vtotal;
    if (h >= profile_.bgate_start) ++v;
    return std::min(v, 263);
}

bool StubVdc::Vblank() const {
    const int h = Hpos(tick_);
    const int v = Vpos(tick_);
    const int start = profile_.vblank_start;
    const int end = profile_.vblank_end;
    if ((v == start && h >= (profile_.hblank_start - 1)) ||
        (v == end && h <= (profile_.hblank_start - 1)))
        return true;
    if (end < start)
        return (v > start || v < end);
    return (v > start && v < end);
}

bool StubVdc::Hblank() const {
    const int h = Hpos(tick_);
    const int start = profile_.hblank_start;
    const int end = profile_.hblank_end;
    if (end < start)
        return (h >= start || h < end);
    return (h >= start && h < end);
}

std::uint8_t StubVdc::FixRegisterMirrors(std::uint8_t offset) {
    // Quad x/y registers are mirrored for each quad.
    if ((offset & 0xC2) == 0x40) offset &= static_cast<std::uint8_t>(~0x0C);
    // Registers A0-AF are mirrored at B0-BF.
    if ((offset & 0xE0) == 0xA0) offset &= static_cast<std::uint8_t>(~0x10);
    return offset;
}

std::uint8_t StubVdc::Read(std::uint8_t raw) {
    const std::uint8_t offset = FixRegisterMirrors(raw);
    switch (offset) {
    case 0xA1: {
        std::uint8_t data = control_status_;
        const int h = Hpos(tick_);
        if (h >= 225 && h < profile_.bgate_start &&
            YBeam(tick_) <= profile_.vblank_start)
            data |= 0x01;
        data |= static_cast<std::uint8_t>(regs_[0xA0] & 0x02);
        // Reading A1 acknowledges the VBL/overlap IRQ on real hardware.
        irq_line_ = false;
        control_status_ &= static_cast<std::uint8_t>(~0xCC);
        return data;
    }
    case 0xA2: {
        const std::uint8_t data = collision_status_;
        collision_status_ = 0;
        return data;
    }
    case 0xA4:
        return (regs_[0xA0] & 0x02) ? static_cast<std::uint8_t>(YBeam(tick_))
                                    : y_latch_;
    case 0xA5:
        return (regs_[0xA0] & 0x02) ? static_cast<std::uint8_t>(XBeam(tick_))
                                    : x_latch_;
    case 0x02: case 0x06: case 0x0A: case 0x0E:
    case 0xA3: case 0xA7: case 0xA8: case 0xA9:
        return 0;
    default:
        break;
    }

    std::uint8_t data = regs_[offset];
    if (offset < 0x80 && (regs_[0xA0] & 0x20))
        data = 0;
    else if (offset >= 0xC0 && (regs_[0xA0] & 0x08))
        data = 0;
    return data;
}

void StubVdc::Write(std::uint8_t raw, std::uint8_t value) {
    const std::uint8_t offset = FixRegisterMirrors(raw);

    // Object x/y/attr registers are inaccessible while display is enabled;
    // shape registers >= 0x80 remain accessible.
    if (offset < 0x80 && (regs_[0xA0] & 0x20)) return;
    // Grid registers are inaccessible while grid is enabled; >= F0 unmapped.
    if ((offset >= 0xC0 && (regs_[0xA0] & 0x08)) || offset >= 0xF0) return;

    if ((offset & 0x83) == 0x03) value &= 0x0F;              // color D4-D7
    if (offset >= 0x10 && (offset & 0x83) == 0x00) value &= static_cast<std::uint8_t>(~0x01);
    if ((offset & 0xF0) == 0xD0) value &= 0x01;              // grid high byte

    switch (offset) {
    case 0xA0:
        if ((regs_[0xA0] & 0x02) && !(value & 0x02)) {
            x_latch_ = static_cast<std::uint8_t>(XBeam(tick_));
            y_latch_ = static_cast<std::uint8_t>(YBeam(tick_));
        }
        break;
    case 0xA1: case 0xA4: case 0xA5:
        return;
    case 0xAA:
        value &= static_cast<std::uint8_t>(~0x40);
        break;
    case 0x03: case 0x07: case 0x0B: case 0x0F:
    case 0xA6: case 0xAB: case 0xAC: case 0xAD: case 0xAE: case 0xAF:
    case 0xC9: case 0xCA: case 0xCB: case 0xCC: case 0xCD: case 0xCE: case 0xCF:
    case 0xD9: case 0xDA: case 0xDB: case 0xDC: case 0xDD: case 0xDE: case 0xDF:
    case 0xEA: case 0xEB: case 0xEC: case 0xED: case 0xEE: case 0xEF:
        return;
    default:
        break;
    }

    regs_[offset] = value;
}

bool StubVdc::SaveState(SnapshotWriter& w) const {
    w.u32(0x56444331u); // "VDC1"
    w.u64(static_cast<std::uint64_t>(tick_));
    w.raw(regs_, sizeof(regs_));
    w.u8(control_status_);
    w.u8(collision_status_);
    w.u8(x_latch_);
    w.u8(y_latch_);
    w.u8(irq_line_ ? 1 : 0);
    return true;
}

bool StubVdc::LoadState(SnapshotReader& r) {
    if (r.u32() != 0x56444331u) return false;
    tick_ = static_cast<Tick>(r.u64());
    r.raw(regs_, sizeof(regs_));
    control_status_ = r.u8();
    collision_status_ = r.u8();
    x_latch_ = r.u8();
    y_latch_ = r.u8();
    irq_line_ = r.u8() != 0;
    return r.ok;
}

} // namespace o2em::ng
