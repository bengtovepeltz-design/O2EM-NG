// O2EM-NG NG-5 G7400 / Videopac+ device conformance suite.
//
// Device-level checks for the Thomson EF9340/EF9341 VPP, its memory and
// character model, transparency/luminance composition with the i8245 VDC,
// save-state determinism, the G7400 machine profile and real BIOS/cartridge
// execution. Standalone: no SDL, frontend, legacy cpu.cpp/vmachine.cpp.
//
// Behavioural reference: MAME 0.289 ef9340_1.cpp and odyssey2.cpp (BSD-3),
// plus the Thomson EF9340/EF9341 register documentation. Independent
// implementation; see Docs/Development/NG-0_Provenance.md.

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "machine.h"
#include "memory/cartridge.h"
#include "timebase.h"
#include "video/ef9340_charset.h"
#include "video/vdc.h"
#include "video/vdc_state.h"
#include "video/vpp.h"

namespace {

int g_checks = 0;
int g_failures = 0;
int g_skips = 0;

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

std::uint8_t Pixel(const o2em::ng::Framebuffer& fb, int x, int y) {
    if (x < 0 || y < 0 || x >= fb.width || y >= fb.height) return 0xEE;
    return fb.pixels[static_cast<std::size_t>(y) * fb.stride + x];
}

// One G7400 device rig: real i8245 VDC + EF934x VPP on one tick timeline.
struct Rig {
    o2em::ng::HardwareProfile profile;
    o2em::ng::Vdc vdc;
    o2em::ng::Vpp vpp;
    bool attached = true;
    std::int64_t rendered_frames = 0;

    explicit Rig(bool b_pin = false, bool attach = true) {
        profile = o2em::ng::MakeG7400Profile();
        attached = attach;
        vdc.Configure(profile);
        vpp.Configure(profile);
        vpp.Reset();
        vpp.SetBPin(b_pin);
        vpp.SetP1(0xFF);
        if (attach) vdc.AttachVpp(&vpp);
        vdc.Reset();
        vdc.SetTick(0);
        vdc.AdvanceTo(0);
    }

    void Cmd(bool b, std::uint8_t v) { vpp.Write(b ? 0x03 : 0x02, v); }
    void Dat(bool b, std::uint8_t v) { vpp.Write(b ? 0x01 : 0x00, v); }
    std::uint8_t ReadDat(bool b) { return vpp.Read(b ? 0x05 : 0x04); }
    std::uint8_t ReadCmd(bool b) { return vpp.Read(b ? 0x07 : 0x06); }

    void SetR(std::uint8_t r) { Dat(false, r); Cmd(true, 0xA0); }
    void SetY0(std::uint8_t y0) { Dat(false, y0); Cmd(true, 0xC0); }
    void SetM(std::uint8_t m) { Dat(false, m); Cmd(true, 0x80); }
    void BeginRow(std::uint8_t y) { Dat(false, y); Cmd(true, 0x00); }
    void LoadX(std::uint8_t x) { Dat(false, x); Cmd(true, 0x40); }
    void Cursor(std::uint8_t x, std::uint8_t y) { BeginRow(y); LoadX(x); }

    void WriteCell(std::uint8_t x, std::uint8_t y, std::uint8_t a,
                   std::uint8_t b) {
        Cursor(x, y);
        SetM(0x00);
        Dat(false, a);
        Dat(true, b);
    }

    void DynamicChar(std::uint8_t x, std::uint8_t y, std::uint8_t a,
                     std::uint8_t code, std::uint8_t slice) {
        WriteCell(x, y, a, code);
        Cursor(x, y);
        SetM(0x80);
        for (int i = 0; i < 10; ++i) {
            Dat(false, slice);
            Dat(true, 0x00);
        }
    }

    void RenderFrame() {
        ++rendered_frames;
        const auto target = rendered_frames * profile.FrameTicks();
        vdc.SetTick(target);
        vdc.AdvanceTo(target);
    }
};

void TestG7400Profile() {
    const auto p = o2em::ng::MakeG7400Profile();
    Check(std::strcmp(p.name, "g7400") == 0, "G7400: profile name");
    CheckEq(static_cast<std::uint64_t>(p.cpu_hz.num), 5911000,
            "G7400: 5.911 MHz 8048 clock");
    CheckEq(static_cast<std::uint64_t>(p.pixel_hz.num), 7093600,
            "G7400: 8.867 MHz video crystal pixel clock");
    CheckEq(static_cast<std::uint64_t>(p.LineTicks()), 4560,
            "G7400: PAL line ticks");
    CheckEq(static_cast<std::uint64_t>(p.FrameTicks()), 1427280,
            "G7400: PAL frame ticks");
    CheckEq(static_cast<std::uint64_t>(p.VblankIrqOffset()), 1107170,
            "G7400: steady-state VBL line 242");
    CheckEq(static_cast<std::uint64_t>(p.FirstVblankIrqOffset()), 1426370,
            "G7400: first VBL line 312");
}

void TestResetState() {
    Rig r;
    CheckEq(r.vpp.R(), 0, "VPP reset: R");
    CheckEq(r.vpp.X(), 0, "VPP reset: X");
    CheckEq(r.vpp.Y(), 0, "VPP reset: Y");
    CheckEq(r.vpp.Y0(), 0, "VPP reset: Y0");
    CheckEq(r.vpp.M(), 0, "VPP reset: M");
    CheckEq(r.vpp.MixI8244(), 0xFF, "VPP reset: i8244 mix");
    CheckEq(r.vpp.MixEf934x(), 0xFF, "VPP reset: EF934x mix");
    CheckEq(r.vpp.CompletedFrames(), 0, "VPP reset: frame counter");
    bool zero = true;
    for (int i = 0; i < 0x400; ++i)
        if (r.vpp.RamA()[i] || r.vpp.RamB()[i]) zero = false;
    for (int i = 0; i < 0x800; ++i)
        if (r.vpp.ExtRam()[i]) zero = false;
    Check(zero, "VPP reset: RAM A/B and extended RAM cleared");
    CheckEq(r.ReadCmd(true), 0, "VPP reset: command B read");
    CheckEq(r.ReadCmd(false), 0, "VPP reset: status/busy clear");
}

void TestRegisterProtocolAndRam() {
    Rig r;
    r.SetR(0x01);
    CheckEq(r.vpp.R(), 0x01, "EF9341: load R");
    r.SetY0(0x07);
    CheckEq(r.vpp.Y0(), 0x07, "EF9341: load Y0");
    r.SetM(0x40);
    CheckEq(r.vpp.M(), 0x40, "EF9341: load M");
    r.BeginRow(0x05);
    CheckEq(r.vpp.Y(), 0x05, "EF9341: begin row sets Y");
    CheckEq(r.vpp.X(), 0x00, "EF9341: begin row clears X");
    r.LoadX(0x13);
    CheckEq(r.vpp.X(), 0x13, "EF9341: load X");
    r.Cmd(true, 0x60);
    CheckEq(r.vpp.X(), 0x14, "EF9341: increment C");

    // Write with increment: (0x13,0x05) and (0x14,0x05).
    r.BeginRow(0x05);
    r.LoadX(0x13);
    r.SetM(0x00);
    r.Dat(false, 0x21);
    r.Dat(true, 0x31);
    r.Dat(false, 0x22);
    r.Dat(true, 0x32);
    CheckEq(r.vpp.X(), 0x15, "EF9341: write increments cursor");

    // Read with increment.
    r.BeginRow(0x05);
    r.LoadX(0x13);
    r.SetM(0x20);
    r.ReadDat(true); // B read latches the cell and returns the previous TB
    CheckEq(r.ReadDat(false), 0x21, "EF9341: read A (attribute)");
    CheckEq(r.ReadDat(true), 0x31, "EF9341: read B (character)");
    CheckEq(r.ReadDat(false), 0x22, "EF9341: read A next cell");
    CheckEq(r.ReadDat(true), 0x32, "EF9341: read B next cell");
    CheckEq(r.vpp.X(), 0x16, "EF9341: read increments cursor");

    // Read without increment returns the same cell twice.
    r.BeginRow(0x05);
    r.LoadX(0x13);
    r.SetM(0x60);
    r.ReadDat(true);
    CheckEq(r.ReadDat(false), 0x21, "EF9341: read A without increment");
    CheckEq(r.ReadDat(true), 0x31, "EF9341: read B without increment");
    CheckEq(r.ReadDat(false), 0x21, "EF9341: read A stays on cell");
    CheckEq(r.ReadDat(true), 0x31, "EF9341: read B stays on cell");

    // Cursor wrap: 40 columns (begin row increments X to 40 after 40 writes).
    r.BeginRow(0x00);
    r.SetM(0x00);
    for (int i = 0; i < 40; ++i) {
        r.Dat(false, 0x00);
        r.Dat(true, static_cast<std::uint8_t>(i));
    }
    CheckEq(r.vpp.X(), 0x00, "EF9341: 40-column row wrap clears X");
    CheckEq(r.vpp.Y(), 0x01, "EF9341: 40-column row wrap increments Y");
}

void TestDynamicCharactersAndExtRam() {
    Rig r;
    // Alphanumeric dynamic char: A attribute fg=7, B code 0xA0.
    r.DynamicChar(0, 0, 0x07, 0xA0, 0xFF);
    r.SetR(0x01);
    r.RenderFrame();
    const auto& layer = r.vpp.Layer();
    bool solid = true;
    for (int y = 15; y < 25; ++y)
        for (int x = 15; x < 23; ++x)
            if (Pixel(layer, x, y) != 0x0F) solid = false;
    Check(solid, "dynamic char: uploaded 0xFF slices render fg 7 + marker");

    // Read slice back through the EF9341.
    r.Cursor(0, 0);
    r.SetM(0xA0);
    r.ReadDat(true);
    CheckEq(r.ReadDat(false), 0xFF, "dynamic char: slice readback");

    // Overwrite one slice: line 7 becomes 0x00.
    r.Cursor(0, 0);
    r.SetM(0x87);
    r.Dat(false, 0x00);
    r.Dat(true, 0x00);
    r.RenderFrame();
    CheckEq(Pixel(r.vpp.Layer(), 15, 22), 0x08,
            "dynamic char: overwritten slice renders background marker");

    // Reset clears dynamic character memory.
    r.vpp.Reset();
    CheckEq(r.vpp.ExtRam()[0], 0, "dynamic char: reset clears extended RAM");
}

void TestCharset() {
    CheckEq(sizeof(o2em::ng::kEf9340Charset), 2560,
            "EF9340: character ROM is 2560 bytes");
    CheckEq(o2em::ng::Crc32(o2em::ng::kEf9340Charset, 2560) == 0xD557A7BFu ? 1 : 0, 1,
            "EF9340: legacy Videopac+ charset CRC D557A7BF");
    Check(o2em::ng::kEf9340Charset[1] == 0x38 &&
              o2em::ng::kEf9340Charset[2] == 0x44,
          "EF9340: charset first glyph bytes match provenance table");
}

void TestRenderModes() {
    // Double width: the same slice is expanded across the 8 pixels.
    Rig r;
    r.WriteCell(0, 0, 0x20 | 0x07, 0xA0);
    r.Cursor(0, 0);
    r.SetM(0x80);
    for (int i = 0; i < 10; ++i) { r.Dat(false, 0x0F); r.Dat(true, 0x00); }
    r.SetR(0x01);
    r.RenderFrame();
    CheckEq(Pixel(r.vpp.Layer(), 15, 15), 0x0F, "VPP mode: double width cell");
    CheckEq(Pixel(r.vpp.Layer(), 22, 15), 0x0F, "VPP mode: double width right edge");

    // Double height: slice 0 is repeated on two scanlines.
    Rig h;
    h.WriteCell(0, 0, 0x10 | 0x07, 0xA0);
    h.Cursor(0, 0);
    h.SetM(0x80);
    for (int i = 0; i < 10; ++i) { h.Dat(false, 0xFF); h.Dat(true, 0x00); }
    h.SetR(0x01);
    h.RenderFrame();
    CheckEq(Pixel(h.vpp.Layer(), 15, 15), 0x0F, "VPP mode: double height line 0");
    CheckEq(Pixel(h.vpp.Layer(), 15, 16), 0x0F, "VPP mode: double height line 1");

    // Inverted attribute.
    Rig inv;
    inv.WriteCell(0, 0, 0x40 | 0x07, 0xA0);
    inv.Cursor(0, 0);
    inv.SetM(0x80);
    for (int i = 0; i < 10; ++i) { inv.Dat(false, 0x00); inv.Dat(true, 0x00); }
    inv.SetR(0x01);
    inv.RenderFrame();
    CheckEq(Pixel(inv.vpp.Layer(), 15, 15), 0x0F,
            "VPP mode: inverted attribute flips glyph bits");

    // Service row: R bit3 enables row 31 above the normal display.
    Rig svc;
    svc.WriteCell(0, 31, 0x07, 0xA0);
    svc.Cursor(0, 31);
    svc.SetM(0x80);
    for (int i = 0; i < 10; ++i) { svc.Dat(false, 0xFF); svc.Dat(true, 0x00); }
    svc.SetR(0x09);
    svc.RenderFrame();
    CheckEq(Pixel(svc.vpp.Layer(), 15, 5), 0x0F,
            "VPP mode: service row enabled");

    // Display disabled: only the service row rule is drawn.
    Rig off;
    off.WriteCell(0, 0, 0x07, 0xA0);
    off.SetR(0x00);
    off.RenderFrame();
    CheckEq(Pixel(off.vpp.Layer(), 15, 15), 0x00,
            "VPP mode: display disabled clears the layer");
}

void TestHorizontalOrientation() {
    // Asymmetric synthetic VPP cell: bit0 must light the leftmost pixel and
    // bit7 the rightmost. This locks the EF9340 cell traversal and rules out a
    // final-frame mirror or a reversed character-bit path.
    Rig left;
    left.WriteCell(0, 0, 0x07, 0xA0);
    left.Cursor(0, 0);
    left.SetM(0x80);
    for (int i = 0; i < 10; ++i) {
        left.Dat(false, i == 0 ? 0x01 : 0x00);
        left.Dat(true, 0x00);
    }
    left.SetR(0x01);
    left.RenderFrame();
    CheckEq(Pixel(left.vpp.Layer(), 15, 15), 0x0F,
            "VPP orientation: bit0 lights leftmost pixel");
    CheckEq(Pixel(left.vpp.Layer(), 22, 15), 0x08,
            "VPP orientation: bit0 leaves rightmost pixel background");

    Rig right;
    right.WriteCell(0, 0, 0x07, 0xA0);
    right.Cursor(0, 0);
    right.SetM(0x80);
    for (int i = 0; i < 10; ++i) {
        right.Dat(false, i == 0 ? 0x80 : 0x00);
        right.Dat(true, 0x00);
    }
    right.SetR(0x01);
    right.RenderFrame();
    CheckEq(Pixel(right.vpp.Layer(), 15, 15), 0x08,
            "VPP orientation: bit7 leaves leftmost pixel background");
    CheckEq(Pixel(right.vpp.Layer(), 22, 15), 0x0F,
            "VPP orientation: bit7 lights rightmost pixel");

    // Static charset bytes are stored most-significant-bit first. Slice 3 of
    // character 0 is 0x40, so the lit pixel must be on the left half.
    Rig charset;
    charset.WriteCell(0, 0, 0x07, 0x00);
    charset.SetR(0x01);
    charset.RenderFrame();
    CheckEq(Pixel(charset.vpp.Layer(), 16, 18), 0x0F,
            "VPP orientation: static charset MSB-first left pixel");
    CheckEq(Pixel(charset.vpp.Layer(), 21, 18), 0x08,
            "VPP orientation: static charset MSB-first right pixel");

    // Adjacent columns advance to the right, not to the left.
    Rig cells;
    cells.WriteCell(0, 0, 0x07, 0xA0);
    cells.WriteCell(1, 0, 0x07, 0xA0);
    cells.Cursor(0, 0);
    cells.SetM(0x00);
    for (int column = 0; column < 2; ++column) {
        for (int i = 0; i < 10; ++i) {
            cells.Dat(false, i == 0 ? 0xFF : 0x00);
            cells.Dat(true, 0x00);
        }
    }
    cells.SetR(0x01);
    cells.RenderFrame();
    CheckEq(Pixel(cells.vpp.Layer(), 15, 15), 0x0F,
            "VPP orientation: column 0 starts at x=15");
    CheckEq(Pixel(cells.vpp.Layer(), 23, 15), 0x0F,
            "VPP orientation: column 1 starts at x=23 (rightward)");
}

void TestComposition() {
    // VDC intensity must survive composition (regression: previously the
    // composite used d|lum and stripped bit 3).
    {
        Rig r(false);
        r.vdc.Write(0x10, 20);  // y
        r.vdc.Write(0x11, 0);   // x
        r.vdc.Write(0x12, 0x80); // ptr -> all-0xFF glyph row
        r.vdc.Write(0x13, 0x0E); // color -> major palette 15
        r.vdc.Write(0xA0, 0x20); // display enabled
        r.RenderFrame();
        Rig bare(false, false);
        bare.vdc.Write(0x10, 20);
        bare.vdc.Write(0x11, 0);
        bare.vdc.Write(0x12, 0x80);
        bare.vdc.Write(0x13, 0x0E);
        bare.vdc.Write(0xA0, 0x20);
        bare.RenderFrame();
        CheckEq(Pixel(r.vdc.Framebuffer(), 12, 20), 15,
                "compose: VDC intensity bit preserved");
    }

    // LUM input P14 (inverted) adds palette bit 3 to VDC output.
    {
        Rig r(false);
        r.vdc.Write(0xA3, 0x38); // background palette 7
        r.vdc.Write(0xA0, 0x20);
        r.vpp.SetP1(0xFF);
        r.RenderFrame();
        CheckEq(Pixel(r.vdc.Framebuffer(), 300, 200), 7,
                "compose: P14 high -> no LUM");
        r.vpp.SetP1(0x7F); // inverted LUM pin low
        r.RenderFrame();
        CheckEq(Pixel(r.vdc.Framebuffer(), 300, 200), 15,
                "compose: P14 low -> LUM bit 3");
    }

    // Cartridge B pin high masks the i8245 decoder unless the EF934x cell
    // carries the "opaque/box" marker.
    {
        Rig box(true);
        box.vdc.Write(0xA3, 0x38); // VDC background 7
        box.vdc.Write(0xA0, 0x20);
        box.SetR(0x03); // display + box attribute
        box.RenderFrame();
        CheckEq(Pixel(box.vdc.Framebuffer(), 300, 200), 0x00,
                "compose: B pin high, box cell masks VDC");

        Rig nobox(false);
        nobox.vdc.Write(0xA3, 0x38);
        nobox.vdc.Write(0xA0, 0x20);
        nobox.SetR(0x03);
        nobox.RenderFrame();
        CheckEq(Pixel(nobox.vdc.Framebuffer(), 300, 200), 7,
                "compose: B pin low keeps VDC decoder enabled");
    }

    // Colour-key mix: clearing i8244 mix bits 4..7 lets the EF934x colour
    // through for VDC colour 4.
    {
        Rig r(false);
        r.vdc.Write(0xA3, 0x48); // VDC background 4
        r.vdc.Write(0xA0, 0x20);
        r.DynamicChar(0, 0, 0x02, 0xA0, 0xFF); // fg 2 + marker
        r.SetR(0x01);
        r.vpp.ExpanderWrite(6, 0x00); // clear mix_i8244 bits 4..7
        r.RenderFrame();
        CheckEq(Pixel(r.vdc.Framebuffer(), 15, 15), 0x02,
                "compose: VDC colour 4 masked -> EF934x colour 2");
        CheckEq(Pixel(r.vdc.Framebuffer(), 300, 200), 0x00,
                "compose: transparent index mapping (empty cell)");
    }

    // VPP update timing: a register write after the top lines were rendered
    // only affects the remaining scanlines (update-on-write).
    {
        Rig r(false);
        r.WriteCell(0, 0, 0x05, 0xA0);
        r.WriteCell(0, 1, 0x02, 0xA0);
        for (int y = 0; y < 2; ++y) {
            r.Cursor(0, static_cast<std::uint8_t>(y));
            r.SetM(0x80);
            for (int i = 0; i < 10; ++i) { r.Dat(false, 0xFF); r.Dat(true, 0x00); }
        }
        r.SetR(0x01);
        r.Cursor(0, 0);
        r.SetM(0xA0);
        r.ReadDat(true);
        r.SetR(0x01);
        // Row 0 is fully rendered at this point (lines 15..24).
        r.vdc.SetTick(24 * r.profile.LineTicks());
        r.vdc.AdvanceTo(24 * r.profile.LineTicks());
        r.Cursor(0, 1);
        r.SetM(0x80);
        r.Dat(false, 0x00);
        r.Dat(true, 0x00);
        r.RenderFrame();
        CheckEq(Pixel(r.vpp.Layer(), 18, 15), 0x0D,
                "timing: earlier scanline keeps previous VPP/VDC state");
        CheckEq(Pixel(r.vpp.Layer(), 18, 25), 0x08,
                "timing: later scanline uses updated VPP state");
    }
}

void TestSaveState() {
    Rig a(false);
    a.WriteCell(0, 0, 0x02, 0xA0);
    a.Cursor(0, 0);
    a.SetM(0x80);
    for (int i = 0; i < 10; ++i) { a.Dat(false, 0xFF); a.Dat(true, 0x00); }
    a.SetR(0x01);
    a.vpp.ExpanderWrite(6, 0x55);
    a.vpp.SetP1(0xEF);
    a.vdc.SetTick(80 * a.profile.LineTicks());
    a.vdc.AdvanceTo(80 * a.profile.LineTicks());

    o2em::ng::SnapshotWriter w;
    Check(a.vpp.SaveState(w), "VPP save-state: save");
    Check(a.vdc.SaveState(w), "VDC save-state: save");
    a.RenderFrame();
    const std::uint64_t h1 = HashFramebuffer(a.vdc.Framebuffer());

    Rig b(false);
    o2em::ng::SnapshotReader r{w.bytes.data(), w.bytes.size(), 0, true};
    Check(b.vpp.LoadState(r), "VPP save-state: restore");
    Check(b.vdc.LoadState(r), "VDC save-state: restore");
    Check(r.ok, "VPP save-state: consumed");
    b.RenderFrame();
    const std::uint64_t h2 = HashFramebuffer(b.vdc.Framebuffer());
    CheckEq(h2, h1, "VPP save-state: resumed frame hash identical");
    CheckEq(b.vpp.CompletedFrames(), a.vpp.CompletedFrames(),
            "VPP save-state: frame counter identical");
    CheckEq(b.vpp.RamA()[0], a.vpp.RamA()[0],
            "VPP save-state: RAM A identical");
    CheckEq(b.vpp.ExtRam()[0], a.vpp.ExtRam()[0],
            "VPP save-state: extended RAM identical");
    CheckEq(b.vpp.MixI8244(), 0x5F, "VPP save-state: colour mix restored");

    // Snapshotting twice mid-frame must be byte-identical.
    o2em::ng::SnapshotWriter w2;
    a.vpp.SaveState(w2);
    Check(w.bytes.size() > w2.bytes.size(), "VPP save-state: deterministic layout");
    o2em::ng::SnapshotWriter w3;
    a.vpp.SaveState(w3);
    Check(w3.bytes == w2.bytes, "VPP save-state: repeat save identical");
}

// ---------------------------------------------------------------------------
// Real machine integration

struct NullHost final : o2em::ng::MachineHost {
    void PresentFrame(const o2em::ng::Framebuffer&) override {}
    void PushAudio(const std::int16_t*, std::size_t) override {}
    void OnFrameComplete(std::uint64_t) override {}
};

std::string Join(const std::string& a, const std::string& b) {
    if (a.empty()) return b;
    if (a.back() == '\\' || a.back() == '/') return a + b;
    return a + "\\" + b;
}

std::vector<std::string> SearchRoots() {
    std::vector<std::string> roots;
    if (const char* env = std::getenv("O2EM_NG_ROOT")) roots.push_back(env);
    roots.push_back(".");
    roots.push_back("D:\\Project\\O2EM-NG");
    const std::string file = __FILE__;
    const std::size_t slash = file.find_last_of("/\\");
    if (slash != std::string::npos) {
        const std::string dir = file.substr(0, slash);
        roots.push_back(Join(Join(dir, ".."), ".."));
    }
    return roots;
}

bool ReadFile(const std::string& path, std::vector<std::uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (n <= 0) {
        std::fclose(f);
        return false;
    }
    out.resize(static_cast<std::size_t>(n));
    const std::size_t got = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return got == out.size();
}

bool LoadAsset(const std::string& relative, std::vector<std::uint8_t>& out) {
    for (const auto& root : SearchRoots())
        if (ReadFile(Join(root, relative), out)) return true;
    return false;
}

std::string LocalCartPath(const std::string& name) {
    const std::string env =
        std::getenv("O2EM_NG_ROM_DIR") ? std::getenv("O2EM_NG_ROM_DIR") : "";
    const std::vector<std::string> dirs = {
        env, "D:\\LaunchBox\\Games\\Philips Videopac+", "Roms", "ROMS"};
    for (const auto& d : dirs) {
        if (d.empty()) continue;
        const std::string p = Join(d, name);
        std::vector<std::uint8_t> tmp;
        if (ReadFile(p, tmp)) return p;
    }
    return {};
}

void TestCartridgeMetadata() {
    std::vector<std::uint8_t> bytes;
    if (LoadAsset("BIOS/G7400.bin", bytes)) {
        CheckEq(o2em::ng::Crc32(bytes.data(), bytes.size()), 0xE20A9F41u,
                "G7400: BIOS CRC identity");
    } else {
        ++g_skips;
        std::printf("SKIP: BIOS/G7400.bin missing\n");
    }
    if (ReadFile(LocalCartPath("vp_60_16.bin"), bytes)) {
        o2em::ng::Cartridge c;
        Check(c.Load(std::move(bytes), "vp_60_16.bin"), "G7400: VP60 loads");
        Check(c.info.b_pin, "G7400: VP60 B pin from cartridge metadata");
    } else {
        ++g_skips;
        std::printf("SKIP: vp_60_16.bin missing\n");
    }
    if (ReadFile(LocalCartPath("pr_tutankham.bin"), bytes)) {
        o2em::ng::Cartridge c;
        Check(c.Load(std::move(bytes), "pr_tutankham.bin"),
              "G7400: Tutankham loads");
        Check(!c.info.b_pin, "G7400: Tutankham has no B pin");
    } else {
        ++g_skips;
        std::printf("SKIP: pr_tutankham.bin missing\n");
    }
}

std::unique_ptr<o2em::ng::Machine> MakeG7400(
    const std::vector<std::uint8_t>& bios,
    const std::vector<std::uint8_t>& cart, const std::string& name,
    NullHost& host) {
    o2em::ng::MachineConfig config;
    config.region = o2em::ng::Region::Pal;
    config.g7400 = true;
    config.vpp_enabled = true;
    auto m = o2em::ng::Machine::Create(config, host);
    if (!m->LoadBiosBytes(bios, "G7400.bin")) return nullptr;
    if (!m->LoadCartridgeBytes(cart, name)) return nullptr;
    m->PowerOn();
    return m;
}

void TestMachineIntegration() {
    std::vector<std::uint8_t> bios;
    if (!LoadAsset("BIOS/G7400.bin", bios)) {
        ++g_skips;
        std::printf("SKIP: G7400 BIOS missing\n");
        return;
    }
    const char* names[] = {"vp_60_16.bin", "vp_59_16.bin", "pr_tutankham.bin"};
    for (const char* name : names) {
        std::vector<std::uint8_t> cart;
        if (!ReadFile(LocalCartPath(name), cart)) {
            ++g_skips;
            std::printf("SKIP: %s missing\n", name);
            continue;
        }
        NullHost first_host;
        auto a = MakeG7400(bios, cart, name, first_host);
        Check(a != nullptr, "machine: G7400 create/load/power-on");
        if (!a) continue;
        CheckEq(std::strcmp(a->Profile().name, "g7400") == 0 ? 1 : 0, 1,
                "machine: G7400 profile selected");
        Check(a->Cart().info.b_pin == (std::strcmp(name, "pr_tutankham.bin") != 0),
              "machine: B pin reaches the VPP");
        for (int i = 0; i < 240; ++i) a->RunFrame();
        CheckEq(a->Bus().IrqLine() == a->Vdc().IrqLine() ? 1 : 0, 1,
                "machine: CPU IRQ is the i8245 VDC line");
        bool vpp_ram_used = false;
        for (int i = 0; i < 0x400; ++i)
            if (a->Vpp().RamA()[i] || a->Vpp().RamB()[i]) vpp_ram_used = true;
        if (std::strcmp(name, "pr_tutankham.bin") != 0)
            Check(vpp_ram_used,
                  "machine: real cartridge drives EF934x through MOVX decode");
        const std::uint64_t h = HashFramebuffer(a->GetFramebuffer());

        std::vector<std::uint8_t> state;
        Check(a->SaveState(state), "machine: save state at frame 240");
        for (int i = 0; i < 30; ++i) a->RunFrame();
        const std::uint64_t h1 = HashFramebuffer(a->GetFramebuffer());
        Check(a->LoadState(state.data(), state.size()),
              "machine: restore state at frame 240");
        for (int i = 0; i < 30; ++i) a->RunFrame();
        const std::uint64_t h2 = HashFramebuffer(a->GetFramebuffer());
        CheckEq(h2, h1, "machine: mid-frame save/restore frame hash identical");

        NullHost second_host;
        auto b = MakeG7400(bios, cart, name, second_host);
        for (int i = 0; i < 240; ++i) b->RunFrame();
        CheckEq(HashFramebuffer(b->GetFramebuffer()), h,
                "machine: independent run is deterministic");
    }

    // A standard G7000 cartridge must still execute on the G7400 profile.
    std::vector<std::uint8_t> stdcart;
    if (ReadFile(LocalCartPath("vp_01.bin"), stdcart)) {
        NullHost host;
        auto m = MakeG7400(bios, stdcart, "vp_01.bin", host);
        Check(m != nullptr, "machine: standard G7000 cart loads under G7400");
        if (m) {
            for (int i = 0; i < 120; ++i) m->RunFrame();
            Check(m->GetFramebuffer().pixels.size() > 0,
                  "machine: standard G7000 cart runs under G7400");
        }
    } else {
        ++g_skips;
        std::printf("SKIP: vp_01.bin missing for G7400 regression\n");
    }
}

} // namespace

int main() {
    std::printf("NG-5 G7400 / Videopac+ device conformance\n");
    TestG7400Profile();
    TestResetState();
    TestRegisterProtocolAndRam();
    TestDynamicCharactersAndExtRam();
    TestCharset();
    TestRenderModes();
    TestHorizontalOrientation();
    TestComposition();
    TestSaveState();
    TestCartridgeMetadata();
    TestMachineIntegration();
    std::printf("\nchecks: %d, failures: %d, skips: %d\n", g_checks,
                g_failures, g_skips);
    if (g_failures == 0) {
        std::printf("RESULT: GREEN - NG-5 G7400 device conformance passed.\n");
        return 0;
    }
    std::printf("RESULT: RED - NG-5 G7400 device conformance failures.\n");
    return 1;
}
