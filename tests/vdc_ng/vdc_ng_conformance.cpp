// O2EM-NG NG-3 real i8244/i8245 VDC conformance suite.
//
// Standalone: builds src/core_ng/video/{vdc,character_rom,i8244,i8245}.cpp.
// No SDL, frontend, legacy cpu.cpp/vmachine.cpp/vdc_stub.cpp.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "timebase.h"
#include "video/character_rom.h"
#include "video/vdc.h"
#include "video/vdc_state.h"

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL  %s\n", what);
    }
}

void CheckEq(std::uint64_t actual, std::uint64_t expected, const char* what) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::printf("FAIL  %s (got %llu, expected %llu)\n", what,
                    static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
    }
}

std::uint64_t Fnv1a(const std::uint8_t* data, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}

std::uint64_t HashFramebuffer(const o2em::ng::Framebuffer& fb) {
    return Fnv1a(fb.pixels.data(), fb.pixels.size());
}

o2em::ng::Vdc MakeVdc(const o2em::ng::HardwareProfile& p) {
    o2em::ng::Vdc v;
    v.Configure(p);
    v.SetTick(0);
    v.AdvanceTo(0);
    return v;
}

o2em::ng::Tick LineTick(const o2em::ng::HardwareProfile& p, int line, int x) {
    return static_cast<o2em::ng::Tick>(line) * p.LineTicks() +
           static_cast<o2em::ng::Tick>(x) * p.pixel_ticks;
}

std::uint8_t Pixel(const o2em::ng::Framebuffer& fb, int x, int y) {
    return fb.pixels[static_cast<std::size_t>(y) * fb.stride + x];
}

void WriteAt(o2em::ng::Vdc& v, o2em::ng::Tick tick, std::uint8_t reg,
             std::uint8_t value) {
    v.SetTick(tick);
    v.Write(reg, value);
}

void TestTimebase() {
    const auto pal = o2em::ng::MakePalProfile();
    const auto ntsc = o2em::ng::MakeNtscProfile();
    Check(pal.htotal == 456 && pal.vtotal == 313 && pal.pixel_ticks == 10,
          "PAL geometry: 456x313, 10 ticks/pixel");
    Check(ntsc.htotal == 455 && ntsc.vtotal == 263 && ntsc.pixel_ticks == 9,
          "NTSC geometry: 455x263, 9 ticks/pixel");
    CheckEq(static_cast<std::uint64_t>(pal.FrameTicks()), 1427280,
            "PAL frame ticks");
    CheckEq(static_cast<std::uint64_t>(ntsc.FrameTicks()), 1076985,
            "NTSC frame ticks");

    o2em::ng::Vdc v = MakeVdc(pal);
    CheckEq(static_cast<std::uint64_t>(v.Hpos(0)), 0, "beam: hpos t=0");
    CheckEq(static_cast<std::uint64_t>(v.Vpos(0)), 0, "beam: vpos t=0");
    CheckEq(static_cast<std::uint64_t>(v.Hpos(pal.LineTicks() - 1)), 455,
            "beam: hpos last pixel of line 0");
    CheckEq(static_cast<std::uint64_t>(v.Vpos(pal.LineTicks())), 1,
            "beam: line wrap");
    CheckEq(static_cast<std::uint64_t>(v.Vpos(pal.FrameTicks())), 0,
            "beam: frame wrap");
    CheckEq(static_cast<std::uint64_t>(v.XBeam(pal.LineTicks() + 2000)), 100,
            "beam: X = hpos/2");
    CheckEq(static_cast<std::uint64_t>(v.YBeam(0)), 263,
            "beam: Y at line 0 before HBL clamps at 263");
    CheckEq(static_cast<std::uint64_t>(v.YBeam(414 * 10)), 1,
            "beam: Y increments at BG sync");
    CheckEq(static_cast<std::uint64_t>(v.YBeam(pal.FrameTicks() - 1)), 263,
            "beam: Y clamps at 263");

    // VBlank/HBlank exact boundaries.
    o2em::ng::Vdc w = MakeVdc(pal);
    auto at = [&](int line, int h) {
        w.SetTick(LineTick(pal, line, h));
        return w;
    };
    Check(!at(242, 364).Vblank() && at(242, 365).Vblank(),
          "PAL VBL starts at line 242 h=365");
    Check(!at(241, 365).Vblank(), "PAL VBL not before start");
    Check(at(312, 365).Vblank() && !at(312, 366).Vblank(),
          "PAL VBL ends at line 312 h=365");
    Check(!at(0, 365).Hblank() && at(0, 366).Hblank() &&
              !at(0, 454).Hblank(),
          "PAL HBL runs 366..453");
    Check(at(0, 453).Hblank(), "PAL HBL includes 453");

    o2em::ng::Vdc n = MakeVdc(ntsc);
    n.SetTick(LineTick(ntsc, 262, 365));
    Check(n.Vblank(), "NTSC VBL at line 262 h=365");
    n.SetTick(LineTick(ntsc, 0, 365));
    Check(n.Vblank(), "NTSC VBL wraps through line 0");
    n.SetTick(LineTick(ntsc, 0, 366));
    Check(!n.Vblank(), "NTSC VBL clears after h=365");
}

void TestRegisterMap() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0x10, 0x11); // major Y CAM d0 not connected -> 0x10
    v.Write(0x14, 0x11);
    v.Write(0xD0, 0xFF); // hgrid high byte d0 only
    v.Write(0xA3, 0xFF); // A3 color register stores all bits
    v.Write(0x13, 0xFF); // major color D4-D7 not connected
    CheckEq(v.Registers()[0x10], 0x10, "register: major Y CAM d0 masked");
    CheckEq(v.Registers()[0x14], 0x10, "register: major Y CAM d0 masked 2");
    CheckEq(v.Registers()[0xD0], 0x01, "register: hgrid high byte masked");
    CheckEq(v.Registers()[0xA3], 0xFF, "register: A3 stores all bits");
    CheckEq(v.Registers()[0x13], 0x0F, "register: major color high nibble masked");

    v.Write(0x45, 0x34); // quad 0 char 1 x mirrored to char 0 x
    CheckEq(v.Registers()[0x41], 0x34, "register: quad x mirror");
    CheckEq(v.Registers()[0x45], 0x00, "register: quad mirror target cleared");

    v.Write(0xB0, 0x12); // A0-AF mirrored at B0-BF
    CheckEq(v.Registers()[0xA0], 0x12, "register: B0 mirrors A0");
    CheckEq(v.Read(0xB0), 0x12, "register: B0 read mirrors A0");

    v.Write(0x02, 0x55);
    v.Write(0x06, 0x55);
    v.Write(0xA3, 0x05);
    CheckEq(v.Read(0x02), 0, "register: 0x02 write-only reads 0");
    CheckEq(v.Read(0x06), 0, "register: 0x06 write-only reads 0");
    CheckEq(v.Read(0xA3), 0, "register: A3 write-only reads 0");
    CheckEq(v.Registers()[0xA3], 0x05, "register: A3 stores");

    v.Write(0xA1, 0x55); // read-only
    v.Write(0xA4, 0x55);
    v.Write(0xA5, 0x55);
    CheckEq(v.Registers()[0xA1], 0, "register: A1 write ignored");
    CheckEq(v.Registers()[0xA4], 0, "register: A4 write ignored");

    v.Write(0xC9, 0x55); // unused hgrid bytes
    v.Write(0xD9, 0x55);
    v.Write(0xEA, 0x55);
    v.Write(0xF0, 0x55);
    CheckEq(v.Registers()[0xC9], 0, "register: unused C9 ignored");
    CheckEq(v.Registers()[0xD9], 0, "register: unused D9 ignored");
    CheckEq(v.Registers()[0xEA], 0, "register: unused EA ignored");
    CheckEq(v.Registers()[0xF0], 0, "register: >=F0 unmapped");

    // Display/grid write guards and read masks.
    v.Write(0xA0, 0x20);
    v.Write(0x00, 0x55);
    v.Write(0x80, 0x5A);
    CheckEq(v.Registers()[0x00], 0, "register: object write blocked by display");
    CheckEq(v.Registers()[0x80], 0x5A, "register: shape write allowed by display");
    CheckEq(v.Read(0x10), 0, "register: object read blocked by display");

    v.Write(0xA0, 0x08);
    v.Write(0xC0, 0x55);
    v.Write(0xE0, 0x55);
    CheckEq(v.Registers()[0xC0], 0, "register: grid write blocked by grid");
    CheckEq(v.Registers()[0xE0], 0, "register: vgrid write blocked by grid");
    CheckEq(v.Read(0xC0), 0, "register: grid read blocked by grid");
}

void TestControlAndBeamLatch() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    const o2em::ng::Tick t1 = LineTick(pal, 100, 50);
    v.SetTick(t1);
    v.Write(0xA0, 0x02); // live beam
    CheckEq(v.Read(0xA5), static_cast<std::uint64_t>(v.XBeam(t1)),
            "A5: live X beam");
    CheckEq(v.Read(0xA4), static_cast<std::uint64_t>(v.YBeam(t1)),
            "A4: live Y beam");

    const o2em::ng::Tick t2 = LineTick(pal, 120, 60);
    v.SetTick(t2);
    v.Write(0xA0, 0x00); // strobe falling edge latches X/Y
    CheckEq(v.Read(0xA5), static_cast<std::uint64_t>(v.XBeam(t2)),
            "A5: latched X beam");
    CheckEq(v.Read(0xA4), static_cast<std::uint64_t>(v.YBeam(t2)),
            "A4: latched Y beam");
    v.SetTick(t2 + 5000);
    CheckEq(v.Read(0xA5), static_cast<std::uint64_t>(v.XBeam(t2)),
            "A5: latch does not follow the beam");
    v.Write(0xA0, 0x02);
    CheckEq(v.Read(0xA5), static_cast<std::uint64_t>(v.XBeam(t2 + 5000)),
            "A5: live again after strobe set");
}

void TestStatusAndIrq() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.AssertVblankIrq();
    Check(v.IrqLine(), "IRQ: VBL asserts line");
    const std::uint8_t s1 = v.Read(0xA1);
    Check((s1 & 0x08) != 0, "A1: VBL status bit");
    Check(!v.IrqLine(), "IRQ: A1 read clears line");
    Check((v.Read(0xA1) & 0x08) == 0, "A1: VBL latch clears on read");
    v.AssertVblankIrq();
    Check((v.Read(0xA1) & 0x08) != 0 && v.IrqLine() == false,
          "A1: VBL reasserts next frame");

    // Position-strobe live status bit.
    v.Write(0xA0, 0x02);
    Check((v.Read(0xA1) & 0x02) != 0, "A1: position strobe bit set");
    v.Write(0xA0, 0x00);
    Check((v.Read(0xA1) & 0x02) == 0, "A1: position strobe bit clear");

    // hstatus bit: line 240, h=225 is inside the hardware window.
    v.SetTick(LineTick(pal, 240, 225));
    Check((v.Read(0xA1) & 0x01) != 0, "A1: hstatus set at h=225");
    v.SetTick(LineTick(pal, 240, 224));
    Check((v.Read(0xA1) & 0x01) == 0, "A1: hstatus clear at h=224");
    v.SetTick(LineTick(pal, 240, 414));
    Check((v.Read(0xA1) & 0x01) == 0, "A1: hstatus clear at BG sync");

    // A1 clears all latched high bits 0xCC.
    v.AssertVblankIrq();
    v.SetTick(LineTick(pal, 10, 10));
    v.Write(0xA0, 0x04);
    v.Write(0xAA, 0x00);
    v.Write(0xA7, 0x01);
    v.Write(0xA8, 0x00);
    v.Write(0xA9, 0x00);
    v.AdvanceTo(LineTick(pal, 10, 10) + pal.LineTicks() * 400);
    const std::uint8_t s2 = v.Read(0xA1);
    Check((s2 & 0x08) != 0, "A1: VBL set before clear");
    v.Read(0xA1);
    Check((v.Read(0xA1) & 0xCC) == 0, "A1: 0xCC cleared after read");
}

void TestCollisionStatus() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0xA2, 0x03); // minor 0+1 mask
    // Two identical minor sprites.
    for (int i = 0; i < 2; ++i) {
        v.Write(0x00 + i * 4 + 0, 100);
        v.Write(0x00 + i * 4 + 1, 100);
        v.Write(0x00 + i * 4 + 2, 0x08 + i * 0x08);
    }
    for (int i = 0; i < 16; ++i) v.Write(0x80 + i, 0xFF); // both sprite shapes
    v.Write(0xA0, 0x20);
    v.AdvanceTo(pal.FrameTicks());
    const std::uint8_t col = v.Read(0xA2);
    Check((col & 0x03) != 0, "A2: minor/minor collision");
    CheckEq(v.Read(0xA2), 0, "A2: read clears collision status");

    // Major priority overlap (two foreground objects on the same pixels).
    o2em::ng::Vdc m = MakeVdc(pal);
    m.Write(0xA0, 0x00);
    m.Write(0xA2, 0x80);
    for (int i = 10; i <= 11; ++i) {
        m.Write(0x10 + i * 4 + 0, 100);
        m.Write(0x10 + i * 4 + 1, 100);
        m.Write(0x10 + i * 4 + 2, 0);
        m.Write(0x10 + i * 4 + 3, 0x02);
    }
    m.Write(0xA0, 0x20);
    m.AdvanceTo(pal.FrameTicks());
    Check((m.Status() & 0x80) != 0, "A1: major priority overlap status");

    // Major/minor collision with the minor bit selected in A2.
    o2em::ng::Vdc mm = MakeVdc(pal);
    mm.Write(0xA0, 0x00);
    mm.Write(0xA2, 0x01);
    mm.Write(0x10, 100);
    mm.Write(0x11, 100);
    mm.Write(0x12, 0);
    mm.Write(0x13, 0x02);
    mm.Write(0x00, 100);
    mm.Write(0x01, 100);
    mm.Write(0x02, 0x08);
    for (int i = 0; i < 16; ++i) mm.Write(0x80 + i, 0xFF);
    mm.Write(0xA0, 0x20);
    mm.AdvanceTo(pal.FrameTicks());
    Check((mm.Read(0xA2) & 0x80) != 0, "A2: major/minor collision");

    // External CX collision.
    o2em::ng::Vdc e = MakeVdc(pal);
    e.Write(0xA0, 0x00);
    e.Write(0xA2, 0x41);
    e.Write(0x00, 100);
    e.Write(0x01, 100);
    e.Write(0x02, 0x08);
    for (int i = 0; i < 8; ++i) e.Write(0x80 + i, 0xFF);
    e.Write(0xA0, 0x30); // display + external collision IRQ
    e.AdvanceTo(LineTick(pal, 100, 359));
    e.ExternalCollision((100 + 5) * 2);
    Check(e.IrqLine(), "CX: external collision asserts IRQ");
    Check((e.Status() & 0x40) != 0, "A1: external collision status");
    Check((e.Read(0xA2) & 0x40) != 0, "A2: external collision bit");
}

void TestBackgroundAndGrid() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA3, 0x38); // background index 7
    v.Write(0xA0, 0x00);
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(v.CompletedFrames(), 1, "frame: one completed frame");
    CheckEq(v.Framebuffer().frame_number, 1, "frame: presented number");
    CheckEq(Pixel(v.Framebuffer(), 0, 0), 7, "background: color index 7");
    CheckEq(Pixel(v.Framebuffer(), 359, 242), 7, "background: full visible area");

    // Horizontal grid line 0 at y=24..26, x starts at 13*2.
    o2em::ng::Vdc g = MakeVdc(pal);
    g.Write(0xA3, 0x01); // grid color index 4
    g.Write(0xC0, 0x01); // hgrid[0][0] bit 0
    g.Write(0xA0, 0x08); // grid enable
    g.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(g.Framebuffer(), 26, 24), 4, "grid: horizontal line start");
    CheckEq(Pixel(g.Framebuffer(), 61, 26), 4, "grid: horizontal line end");
    CheckEq(Pixel(g.Framebuffer(), 26, 27), 0, "grid: horizontal line height");

    // Vertical grid line 0 at x=26/28, y=24..47.
    o2em::ng::Vdc vg = MakeVdc(pal);
    vg.Write(0xA3, 0x01);
    vg.Write(0xE0, 0x01); // vgrid[0] bit 0
    vg.Write(0xA0, 0x08);
    vg.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(vg.Framebuffer(), 26, 24), 4, "grid: vertical line start");
    CheckEq(Pixel(vg.Framebuffer(), 26, 47), 4, "grid: vertical line end");
    CheckEq(Pixel(vg.Framebuffer(), 26, 48), 0, "grid: vertical line end+1");

    // Grid dots.
    o2em::ng::Vdc gd = MakeVdc(pal);
    gd.Write(0xA3, 0x01);
    gd.Write(0xA0, 0x48); // grid + dots
    gd.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(gd.Framebuffer(), 26, 24), 4, "grid: dot at first node");
    CheckEq(Pixel(gd.Framebuffer(), 26, 27), 0, "grid: dot height");
}

void TestMinorSprites() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0x00, 100); // y
    v.Write(0x01, 100); // x
    v.Write(0x02, 0x08); // color index 9
    for (int i = 0; i < 16; ++i) v.Write(0x80 + i, 0xFF); // both sprite shapes
    v.Write(0xA0, 0x20);
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(v.Framebuffer(), 210, 100), 9, "minor: first pixel");
    CheckEq(Pixel(v.Framebuffer(), 223, 107), 9, "minor: last pixel");
    CheckEq(Pixel(v.Framebuffer(), 209, 100), 0, "minor: left clip");
    CheckEq(Pixel(v.Framebuffer(), 226, 100), 0, "minor: right edge");

    // Zoom doubles the sprite.
    o2em::ng::Vdc z = MakeVdc(pal);
    z.Write(0xA0, 0x00);
    z.Write(0x00, 100);
    z.Write(0x01, 100);
    z.Write(0x02, 0x0C); // zoom + color 9
    for (int i = 0; i < 8; ++i) z.Write(0x80 + i, 0xFF);
    z.Write(0xA0, 0x20);
    z.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(z.Framebuffer(), 210, 100), 9, "minor: zoom first pixel");
    CheckEq(Pixel(z.Framebuffer(), 241, 131), 9, "minor: zoom last pixel");
    CheckEq(Pixel(z.Framebuffer(), 242, 100), 0, "minor: zoom right edge");

    // Bottom clipping.
    o2em::ng::Vdc c = MakeVdc(pal);
    c.Write(0xA0, 0x00);
    c.Write(0x00, 240);
    c.Write(0x01, 100);
    c.Write(0x02, 0x08);
    for (int i = 0; i < 8; ++i) c.Write(0x80 + i, 0xFF);
    c.Write(0xA0, 0x20);
    c.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(c.Framebuffer(), 210, 240), 9, "minor: bottom visible row");
    CheckEq(Pixel(c.Framebuffer(), 210, 242), 9, "minor: bottom clipped row");
}

void TestMajorObjects() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0x10, 100); // y
    v.Write(0x11, 100); // x
    v.Write(0x12, 0);   // ptr
    v.Write(0x13, 0x02); // color index 9
    v.Write(0xA0, 0x20);
    v.AdvanceTo(pal.FrameTicks());
    const std::uint8_t row = o2em::ng::kI8244CharacterRom[50];
    for (int cx = 0; cx < 8; ++cx) {
        const int expected = ((row >> (cx ^ 7)) & 1) ? 9 : 0;
        CheckEq(Pixel(v.Framebuffer(), 210 + cx * 2, 100),
                static_cast<std::uint64_t>(expected),
                "major: character row pixel");
    }
    CheckEq(Pixel(v.Framebuffer(), 226, 100), 0, "major: right edge");

    // Y CAM d0 is masked: y=0x11 renders at 0x10.
    o2em::ng::Vdc y = MakeVdc(pal);
    y.Write(0xA0, 0x00);
    y.Write(0x10, 0x11);
    y.Write(0x11, 100);
    y.Write(0x12, 0);
    y.Write(0x13, 0x02);
    y.Write(0xA0, 0x20);
    y.AdvanceTo(pal.FrameTicks());
    const std::uint8_t row16 = o2em::ng::kI8244CharacterRom[8];
    CheckEq(Pixel(y.Framebuffer(), 210, 16),
            static_cast<std::uint64_t>(((row16 >> 7) & 1) ? 9 : 0),
            "major: Y CAM d0 masked");

    // NTSC skips major objects with y < 0x0E; PAL draws them.
    const auto ntsc = o2em::ng::MakeNtscProfile();
    o2em::ng::Vdc np = MakeVdc(ntsc);
    np.Write(0xA0, 0x00);
    np.Write(0x10, 0x0D);
    np.Write(0x11, 100);
    np.Write(0x12, 0);
    np.Write(0x13, 0x02);
    np.Write(0xA0, 0x20);
    np.AdvanceTo(ntsc.FrameTicks());
    CheckEq(Pixel(np.Framebuffer(), 210, 13), 0, "major: NTSC y<0x0E skipped");
    o2em::ng::Vdc pp = MakeVdc(pal);
    pp.Write(0xA0, 0x00);
    pp.Write(0x10, 0x0D);
    pp.Write(0x11, 100);
    pp.Write(0x12, 0);
    pp.Write(0x13, 0x02);
    pp.Write(0xA0, 0x20);
    pp.AdvanceTo(pal.FrameTicks());
    const std::uint8_t row6 = o2em::ng::kI8244CharacterRom[6];
    CheckEq(Pixel(pp.Framebuffer(), 210, 13),
            static_cast<std::uint64_t>(((row6 >> 7) & 1) ? 9 : 0),
            "major: PAL y=0x0D drawn");
}

void TestQuads() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    // Quad 0, four characters. The quad X/Y registers are mirrored to the
    // first character, so all four writes carry the same origin.
    for (int j = 0; j < 4; ++j) {
        v.Write(0x40 + j * 4 + 0, 100);
        v.Write(0x40 + j * 4 + 1, 80);
        v.Write(0x40 + j * 4 + 2, 0);
        v.Write(0x40 + j * 4 + 3, 0x02);
    }
    v.Write(0xA0, 0x20);
    v.AdvanceTo(pal.FrameTicks());
    const std::uint8_t row = o2em::ng::kI8244CharacterRom[50];
    const int base_x = (80 + 5) * 2;
    for (int j = 0; j < 4; ++j) {
        const int x = base_x + j * 32;
        const int expected = ((row >> 7) & 1) ? 9 : 0;
        CheckEq(Pixel(v.Framebuffer(), x, 100),
                static_cast<std::uint64_t>(expected),
                "quad: character position");
    }
    CheckEq(Pixel(v.Framebuffer(), base_x + 16, 100), 0,
            "quad: 16-pixel gap between characters");
    CheckEq(Pixel(v.Framebuffer(), base_x + 32, 100),
            static_cast<std::uint64_t>(((row >> 7) & 1) ? 9 : 0),
            "quad: 32-pixel character pitch");
}

void TestUpdateOnWrite() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA3, 0x00); // background 0
    v.Write(0xA0, 0x00);
    v.AdvanceTo(LineTick(pal, 100, 100));
    CheckEq(Pixel(v.LiveFramebuffer(), 99, 99), 0, "update: before write old");
    WriteAt(v, LineTick(pal, 100, 100), 0xA3, 0x38); // background 7
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(v.Framebuffer(), 99, 99), 0, "update: completed line old");
    CheckEq(Pixel(v.Framebuffer(), 99, 100), 0, "update: before write x");
    CheckEq(Pixel(v.Framebuffer(), 100, 100), 7, "update: after write x");
    CheckEq(Pixel(v.Framebuffer(), 359, 100), 7, "update: rest of line new");
    CheckEq(Pixel(v.Framebuffer(), 0, 101), 7, "update: next line new");

    // Grid enable mid-frame only affects subsequent lines.
    o2em::ng::Vdc g = MakeVdc(pal);
    g.Write(0xA3, 0x01);
    g.Write(0xC0, 0x01);
    g.Write(0xA0, 0x00);
    g.AdvanceTo(LineTick(pal, 24, 0));
    g.Write(0xA0, 0x08);
    g.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(g.Framebuffer(), 26, 24), 4, "update: grid after enable");
    CheckEq(Pixel(g.Framebuffer(), 26, 23), 0, "update: no grid before line 24");
}

void TestSaveRestore() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc a = MakeVdc(pal);
    a.Write(0xA3, 0x18);
    a.Write(0xA0, 0x00);
    a.Write(0x00, 100);
    a.Write(0x01, 100);
    a.Write(0x02, 0x08);
    for (int i = 0; i < 8; ++i) a.Write(0x80 + i, 0xFF);
    a.Write(0xA0, 0x28);
    a.AdvanceTo(LineTick(pal, 60, 200));
    o2em::ng::SnapshotWriter w;
    Check(a.SaveState(w), "save-state: save");
    a.AdvanceTo(pal.FrameTicks());
    const std::uint64_t h1 = HashFramebuffer(a.Framebuffer());
    const std::uint8_t s1 = a.Status();

    o2em::ng::Vdc b = MakeVdc(pal);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(b.LoadState(r), "save-state: restore");
    b.AdvanceTo(pal.FrameTicks());
    const std::uint64_t h2 = HashFramebuffer(b.Framebuffer());
    CheckEq(h2, h1, "save-state: framebuffer hash identical");
    CheckEq(b.Status(), s1, "save-state: status identical");
    CheckEq(b.CompletedFrames(), a.CompletedFrames(),
            "save-state: frame count identical");

    // Determinism across two independent runs.
    o2em::ng::Vdc c = MakeVdc(pal);
    c.Write(0xA3, 0x10);
    c.Write(0xC0, 0xFF);
    c.Write(0xE0, 0xFF);
    c.Write(0xA0, 0x08);
    c.AdvanceTo(pal.FrameTicks());
    o2em::ng::Vdc d = MakeVdc(pal);
    d.Write(0xA3, 0x10);
    d.Write(0xC0, 0xFF);
    d.Write(0xE0, 0xFF);
    d.Write(0xA0, 0x08);
    d.AdvanceTo(pal.FrameTicks());
    CheckEq(HashFramebuffer(c.Framebuffer()), HashFramebuffer(d.Framebuffer()),
            "frame: deterministic render hash");
    CheckEq(c.Framebuffer().width, 360, "frame: width");
    CheckEq(c.Framebuffer().height, 243, "frame: height");
    CheckEq(c.Framebuffer().stride, 360, "frame: stride");
}

void TestPalNtscFrame() {
    const auto pal = o2em::ng::MakePalProfile();
    const auto ntsc = o2em::ng::MakeNtscProfile();
    o2em::ng::Vdc p = MakeVdc(pal);
    p.AdvanceTo(pal.FrameTicks());
    o2em::ng::Vdc n = MakeVdc(ntsc);
    n.AdvanceTo(ntsc.FrameTicks());
    CheckEq(p.CompletedFrames(), 1, "PAL: frame completed");
    CheckEq(n.CompletedFrames(), 1, "NTSC: frame completed");
    CheckEq(p.Framebuffer().width, 360, "PAL: visible width");
    CheckEq(n.Framebuffer().height, 243, "NTSC: visible height");
}

void TestSoundShift() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x04); // sound IRQ enable
    v.Write(0xA7, 0x00);
    v.Write(0xA8, 0x00);
    v.Write(0xA9, 0x01);
    v.AdvanceTo(pal.FrameTicks() * 2);
    Check((v.Status() & 0x04) != 0, "sound: 24-shift IRQ status");
    Check(v.IrqLine(), "sound: IRQ line asserted");
    v.Read(0xA1);
    Check(!v.IrqLine(), "sound: A1 clears IRQ");
}

void TestExternalCollision() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0xA2, 0x40);
    v.Write(0x00, 100);
    v.Write(0x01, 100);
    v.Write(0x02, 0x08);
    for (int i = 0; i < 16; ++i) v.Write(0x80 + i, 0xFF); // both sprite shapes
    v.Write(0xA0, 0x30); // display + external collision IRQ
    v.AdvanceTo(LineTick(pal, 100, 359));
    v.ExternalCollision((100 + 5) * 2);
    Check((v.Status() & 0x40) != 0, "CX: external status bit");
    Check(v.IrqLine(), "CX: external IRQ");
}

void TestRasterBoundary() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA3, 0x38);
    v.Write(0xA0, 0x00);
    v.AdvanceTo(LineTick(pal, 242, 360));
    CheckEq(v.LiveFramebuffer().pixels[242 * 360 + 359], 7,
            "raster: last visible pixel rendered before boundary");
    v.AdvanceTo(pal.FrameTicks() - 1);
    CheckEq(v.CompletedFrames(), 0, "raster: frame not completed in VBL");
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(v.CompletedFrames(), 1, "raster: frame completed at boundary");
}

void TestScanlineCollisionIsolation() {
    const auto pal = o2em::ng::MakePalProfile();
    // A single sprite must not collide with itself across scanlines. MAME
    // clears the collision map at the start of every scanline; this catches
    // the old frame-global accumulation bug.
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0xA2, 0xFF);
    v.Write(0x00, 100);
    v.Write(0x01, 100);
    v.Write(0x02, 0x08);
    for (int i = 0; i < 8; ++i) v.Write(0x80 + i, 0xFF);
    v.Write(0xA0, 0x20);
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(v.Read(0xA2), 0, "collision: single sprite no self-collision");

    // Two sprites at non-overlapping x must not collide.
    o2em::ng::Vdc w = MakeVdc(pal);
    w.Write(0xA0, 0x00);
    w.Write(0xA2, 0xFF);
    w.Write(0x00, 100); w.Write(0x01, 100); w.Write(0x02, 0x08);
    w.Write(0x04, 100); w.Write(0x05, 140); w.Write(0x06, 0x10);
    for (int i = 0; i < 16; ++i) w.Write(0x80 + i, 0xFF);
    w.Write(0xA0, 0x20);
    w.AdvanceTo(pal.FrameTicks());
    CheckEq(w.Read(0xA2) & 0x03, 0, "collision: separated sprites none");

    // Two sprites at the same x must collide.
    o2em::ng::Vdc o = MakeVdc(pal);
    o.Write(0xA0, 0x00);
    o.Write(0xA2, 0xFF);
    o.Write(0x00, 100); o.Write(0x01, 100); o.Write(0x02, 0x08);
    o.Write(0x04, 100); o.Write(0x05, 100); o.Write(0x06, 0x10);
    for (int i = 0; i < 16; ++i) o.Write(0x80 + i, 0xFF);
    o.Write(0xA0, 0x20);
    o.AdvanceTo(pal.FrameTicks());
    Check((o.Read(0xA2) & 0x03) != 0, "collision: overlapping sprites detected");
}

void TestMidFrameDisplayDisable() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc v = MakeVdc(pal);
    v.Write(0xA0, 0x00);
    v.Write(0x00, 100);
    v.Write(0x01, 100);
    v.Write(0x02, 0x08);
    for (int i = 0; i < 8; ++i) v.Write(0x80 + i, 0xFF);
    v.Write(0xA0, 0x20);
    const o2em::ng::Tick mid = LineTick(pal, 110, 0);
    v.AdvanceTo(mid);
    CheckEq(Pixel(v.LiveFramebuffer(), 210, 100), 9,
            "mid-frame: sprite visible before disable");
    WriteAt(v, mid, 0xA0, 0x00);
    v.AdvanceTo(pal.FrameTicks());
    CheckEq(Pixel(v.Framebuffer(), 210, 100), 9,
            "mid-frame: already-rendered scanlines kept");
    CheckEq(Pixel(v.Framebuffer(), 210, 110), 0,
            "mid-frame: scanlines after disable blank");
    CheckEq(Pixel(v.Framebuffer(), 210, 107), 9,
            "mid-frame: last scanline before disable kept");
}

void TestSaveRestorePendingIrq() {
    const auto pal = o2em::ng::MakePalProfile();
    o2em::ng::Vdc a = MakeVdc(pal);
    a.AssertVblankIrq();
    o2em::ng::SnapshotWriter w;
    Check(a.SaveState(w), "save: pending irq snapshot");
    o2em::ng::Vdc b = MakeVdc(pal);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(b.LoadState(r), "save: pending irq restore");
    Check(b.IrqLine(), "save: irq line restored");
    Check((b.Status() & 0x08) != 0, "save: vbl status restored");
    Check((b.Read(0xA1) & 0x08) != 0, "save: A1 still reports VBL");
    Check(!b.IrqLine(), "save: A1 clears restored irq");
}

void TestPalNtscSceneHashes() {
    auto render = [](const o2em::ng::HardwareProfile& p) {
        o2em::ng::Vdc v = MakeVdc(p);
        v.Write(0xA0, 0x00);
        v.Write(0xA3, 0x29);
        v.Write(0xC0, 0x05);
        v.Write(0xE0, 0x03);
        v.Write(0x00, 90);
        v.Write(0x01, 80);
        v.Write(0x02, 0x0C);
        for (int i = 0; i < 8; ++i) v.Write(0x80 + i, 0xAA);
        v.Write(0x10, 120);
        v.Write(0x11, 40);
        v.Write(0x12, 0);
        v.Write(0x13, 0x06);
        v.Write(0x40, 60);
        v.Write(0x41, 56);
        for (int j = 0; j < 4; ++j) {
            v.Write(0x40 + j * 4 + 2, static_cast<std::uint8_t>(j * 8));
            v.Write(0x40 + j * 4 + 3, 0x0B);
        }
        v.Write(0xA0, 0x28);
        v.AdvanceTo(p.FrameTicks());
        return HashFramebuffer(v.Framebuffer());
    };
    const auto pal = o2em::ng::MakePalProfile();
    const auto ntsc = o2em::ng::MakeNtscProfile();
    const std::uint64_t ph1 = render(pal);
    const std::uint64_t ph2 = render(pal);
    const std::uint64_t nh1 = render(ntsc);
    CheckEq(ph1, ph2, "hash: PAL scene deterministic");
    Check(ph1 != 0 && nh1 != 0, "hash: scene hashes nonzero");
    Check(ph1 != nh1, "hash: PAL and NTSC scenes differ");
}

} // namespace

int main() {
    std::printf("O2EM-NG NG-3 i8244/i8245 VDC conformance\n");
    std::printf("core: src/core_ng/video/{vdc,character_rom,i8244,i8245}\n\n");

#define RUN(fn) do { std::printf("[TEST] %s\n", #fn); std::fflush(stdout); fn(); } while (0)
    RUN(TestTimebase);
    RUN(TestRegisterMap);
    RUN(TestControlAndBeamLatch);
    RUN(TestStatusAndIrq);
    RUN(TestCollisionStatus);
    RUN(TestBackgroundAndGrid);
    RUN(TestMinorSprites);
    RUN(TestMajorObjects);
    RUN(TestQuads);
    RUN(TestUpdateOnWrite);
    RUN(TestSaveRestore);
    RUN(TestPalNtscFrame);
    RUN(TestSoundShift);
    RUN(TestExternalCollision);
    RUN(TestRasterBoundary);
    RUN(TestScanlineCollisionIsolation);
    RUN(TestMidFrameDisplayDisable);
    RUN(TestSaveRestorePendingIrq);
    RUN(TestPalNtscSceneHashes);

    std::printf("\nchecks: %d, failures: %d\n", g_checks, g_failures);
    if (g_failures == 0)
        std::printf("RESULT: GREEN - NG-3 VDC conformance passed.\n");
    else
        std::printf("RESULT: RED - NG-3 VDC conformance failures.\n");
    return g_failures == 0 ? 0 : 1;
}
