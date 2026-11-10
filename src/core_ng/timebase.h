#pragma once
// O2EM-NG authoritative machine timebase.
//
// There is exactly one monotonic machine clock. Its unit is the NG tick.
//
// The Intel 8048 divides its oscillator by 15 to produce one machine cycle
// (the MCS-48 core's native execution unit). The NG tick is chosen so that
// one 8048 machine cycle is an exact integer number of ticks and both
// console families also land on exact pixel boundaries:
//
//   180 ticks == 1 Intel MCS-48 machine cycle (15 oscillator periods)
//
//   PAL  (i8245): oscillator 17.734476/3 MHz, pixel = 6/5 * oscillator.
//                 One machine cycle = 18 pixels = 180 ticks (10 ticks/pixel).
//   NTSC (i8244): oscillator 7.15909*3/4 MHz, pixel = 4/3 * oscillator.
//                 One machine cycle = 20 pixels = 180 ticks (9 ticks/pixel).
//
// The previous 12-tick value conflated the oscillator with the machine cycle
// and ran the CPU 15x too fast relative to video; that is corrected here.
//
// No floating point is used anywhere in the timing path. Frame, line and
// pixel events are scheduled on exact integer tick boundaries.

#include <cstdint>

namespace o2em::ng {

using Tick = std::int64_t; // absolute machine timestamp in 1/180 CPU-machine-cycle units

constexpr int kTicksPerCpuCycle = 180;

struct Rational {
    std::int64_t num = 0;
    std::int64_t den = 1;

    constexpr Rational() = default;
    constexpr Rational(std::int64_t n, std::int64_t d) : num(n), den(d) {}

    constexpr bool IsZero() const { return num == 0; }
    constexpr double ToDouble() const {
        return static_cast<double>(num) / static_cast<double>(den);
    }
};

enum class Region { Auto, Pal, Ntsc };

// One hardware profile is a single coherent machine description. PAL/NTSC
// are two instances of the same architecture, not two timing code paths.
struct HardwareProfile {
    Region region = Region::Pal;
    const char* name = "pal";

    // 8048 oscillator frequency (the core divides it by 15 for machine cycles).
    Rational cpu_hz{5911492, 1};

    // i8244/i8245 pixel clock (2x the i824x device clock, as in MAME's
    // screen raw clock).
    Rational pixel_hz{70937904, 10};

    // Integer tick periods. tick == 1/180 CPU machine cycle.
    std::int64_t pixel_ticks = 10;

    int htotal = 456;
    int vtotal = 313;
    int vblank_start = 242;
    int vblank_end = 312;
    int hblank_start = 366;
    int hblank_end = 454;
    int bgate_start = 414;

    int visible_x = 0;
    int visible_y = 0;
    int visible_w = 360;
    int visible_h = 243;

    constexpr std::int64_t LineTicks() const { return std::int64_t(htotal) * pixel_ticks; }
    constexpr std::int64_t FrameTicks() const { return std::int64_t(vtotal) * LineTicks(); }
    constexpr std::int64_t CpuCyclesPerFrameNumerator() const { return FrameTicks(); }

    // Tick of the steady-state VBL IRQ assertion within a frame.
    //
    // The NG frame origin is the first visible scanline (vpos 0), exactly the
    // coordinate used by the renderer. VBLANK runs from vblank_start (242 on
    // PAL) to vblank_end (312). MAME schedules the i824x IRQ with
    // screen().time_until_pos(vblank_start, hblank_start - 1); because MAME
    // measures positions from VBLANK start, the steady-state event lands at
    // NG vpos 242. Scheduling every event at 312 instead put the IRQ at the
    // *end* of VBLANK, which made games that rebuild the display list during
    // VBLANK run their update in the visible top of the frame (Tutankham+,
    // Frogger). One frame still has exactly one VBL IRQ.
    constexpr std::int64_t VblankIrqOffset() const {
        return std::int64_t(vblank_start) * LineTicks() +
               std::int64_t(hblank_start - 1) * pixel_ticks;
    }

    // Tick of the very first VBL IRQ after power-on/reset.
    //
    // MAME's screen origin at reset is the first visible line, so the first
    // time_until_pos() call walks to the end of the (partial) first frame,
    // absolute line 312 on PAL / 262 on NTSC. Every later frame uses the
    // steady-state phase above. The two-phase model is required by the NG-2
    // MAME prefix differential; a fixed 312 offset (the old behaviour) broke
    // the steady-state games, and a fixed 242 offset broke the reset prefix.
    constexpr std::int64_t FirstVblankIrqOffset() const {
        const std::int64_t absolute_line =
            (std::int64_t(vblank_start) + vtotal - visible_h) % vtotal;
        return absolute_line * LineTicks() +
               std::int64_t(hblank_start - 1) * pixel_ticks;
    }
};

inline HardwareProfile MakeNtscProfile() {
    HardwareProfile p;
    p.region = Region::Ntsc;
    p.name = "ntsc";
    // Philips/Magnavox NTSC consoles: 7.15909MHz crystal.
    // CPU = 3/4 crystal = 5,369,317.5 Hz; pixel = crystal = 7,159,090 Hz.
    p.cpu_hz = Rational(10738635, 2);
    p.pixel_hz = Rational(7159090, 1);
    p.pixel_ticks = 9;
    p.htotal = 455;
    p.vtotal = 263;
    p.vblank_start = 242;
    p.vblank_end = 0;
    p.hblank_start = 366;
    p.hblank_end = 453;
    p.bgate_start = 413;
    p.visible_w = 360;
    p.visible_h = 243;
    return p;
}

inline HardwareProfile MakePalProfile() {
    HardwareProfile p;
    p.region = Region::Pal;
    p.name = "pal";
    // Philips G7000 (Europe): 17.734476MHz crystal.
    // CPU = crystal/3; i8245 pixel = 2*crystal/5.
    p.cpu_hz = Rational(17734476, 3);
    p.pixel_hz = Rational(35468952, 5);
    p.pixel_ticks = 10;
    p.htotal = 456;
    p.vtotal = 313;
    p.vblank_start = 242;
    p.vblank_end = 312;
    p.hblank_start = 366;
    p.hblank_end = 454;
    p.bgate_start = 414;
    p.visible_w = 360;
    p.visible_h = 243;
    return p;
}

inline HardwareProfile MakePalC52Profile() {
    HardwareProfile p = MakePalProfile();
    p.name = "pal-c52";
    p.cpu_hz = Rational(17812000, 3);
    p.pixel_hz = Rational(35624000, 5);
    return p;
}

// G7400 / Videopac+ machine profile.
//
// The G7400 uses a 5.911 MHz 8048 clock and an 8.867 MHz video crystal
// (i8245 = (8.867*2)/5 MHz, i824x pixel clock = 2x that). The NG architecture
// keeps the 8048 machine cycle as the authoritative integer tick (180 ticks)
// and derives the raster from it at 18 pixels/cycle, exactly like the G7000
// PAL profile. The 0.0056% difference against the nominal G7400 crystals is
// below one pixel over a frame and never accumulates because the frame grid
// is fixed. The VDC/VPP therefore share the single NG machine timeline.
inline HardwareProfile MakeG7400Profile() {
    HardwareProfile p = MakePalProfile();
    p.name = "g7400";
    p.cpu_hz = Rational(5911000, 1);
    p.pixel_hz = Rational(7093600, 1);
    return p;
}

inline HardwareProfile MakeProfile(Region region) {
    switch (region) {
    case Region::Ntsc: return MakeNtscProfile();
    case Region::Pal:  return MakePalProfile();
    case Region::Auto:
    default:           return MakePalProfile();
    }
}

// Helpers used by the scheduler and devices. All are integer-exact.
inline std::int64_t CpuCyclesToTicks(std::int64_t cycles) {
    return cycles * kTicksPerCpuCycle;
}

inline std::int64_t TicksToCpuCyclesFloor(Tick tick) {
    if (tick <= 0) return 0;
    return tick / kTicksPerCpuCycle;
}

// Rational phase helper: returns floor(tick * num / den) without floating
// point. Used to derive device-local pixel/line positions from global ticks.
inline std::int64_t MulDivFloor(std::int64_t value, std::int64_t num,
                                std::int64_t den) {
    if (den == 0) return 0;
    if (value >= 0) return (value / den) * num + ((value % den) * num) / den;
    return -(((-value) / den) * num + (((-value) % den) * num) / den);
}

} // namespace o2em::ng
