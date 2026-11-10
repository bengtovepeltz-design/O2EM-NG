#pragma once
// O2EM-NG Thomson EF9340 (VIN) + EF9341 (GEN) Videopac+ device.
//
// The CPU talks to the EF9341 through MOVX addresses 0..7 while P1 bit5 is
// low; P1 bit4 is the external LUM input. The EF9340 owns the 2x1KB display
// RAM (A attributes, B character codes), a 2KB extended/dynamic memory and a
// 256x10 character generator. Rendering is per scanline on the NG machine
// timeline (no independent clock). The layer uses MAME's bit3 marker: set for
// normal cells, clear for "box"/opaque cells.
//
// Behavioural reference: MAME 0.289 src/devices/video/ef9340_1.cpp (BSD-3)
// and the G7400 composition in src/mame/philips/odyssey2.cpp. Independent
// implementation; see Docs/Development/NG-0_Provenance.md.

#include <cstdint>

#include "../state.h"
#include "../timebase.h"
#include "framebuffer.h"
#include "vdc.h"

namespace o2em::ng {

class Vpp final {
public:
    void Configure(const HardwareProfile& profile);
    void Reset();

    void SetTick(Tick tick) { tick_ = tick; }
    Tick CurrentTick() const { return tick_; }
    void AdvanceTo(Tick tick);

    // Cartridge pin B: plus cartridges tie it high so the EF934x can mask the
    // i8244 decoder. Standard G7000 cartridges leave it low.
    void SetBPin(bool value) { b_pin_ = value; }
    void SetP1(std::uint8_t p1) { p1_ = p1; }
    bool BPin() const { return b_pin_; }

    // MOVX interface (only called while P1 bit5 is low).
    std::uint8_t Read(std::uint8_t address);
    void Write(std::uint8_t address, std::uint8_t value);

    // 8243 expander output ports P4..P7 (colour mix keys).
    void ExpanderWrite(int port, std::uint8_t value);

    // Called by the VDC once per completed scanline (update-on-write).
    void CompositeLine(int screen_line, Vdc* vdc);

    // EF9341 primitive interface.
    void Ef9341Write(bool command, bool b, std::uint8_t data);
    std::uint8_t Ef9341Read(bool command, bool b);

    // Composite the completed VDC frame with the completed VPP layer.
    // p1 is the machine's current P1 output (bit4 = LUM, inverted).
    void Compose(const Framebuffer& vdc, std::uint8_t p1, Framebuffer& out,
                 Vdc* vdc_device);

    const Framebuffer& Layer() const { return layer_; }
    const Framebuffer& Composite() const { return composite_; }
    std::uint64_t CompletedFrames() const { return completed_frames_; }

    std::uint8_t MixI8244() const { return mix_i8244_; }
    std::uint8_t MixEf934x() const { return mix_ef934x_; }
    std::uint8_t X() const { return x_; }
    std::uint8_t Y() const { return y_; }
    std::uint8_t Y0() const { return y0_; }
    std::uint8_t R() const { return r_; }
    std::uint8_t M() const { return m_; }
    const std::uint8_t* RamA() const { return ram_a_; }
    const std::uint8_t* RamB() const { return ram_b_; }
    const std::uint8_t* ExtRam() const { return extram_; }

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

private:
    static std::uint16_t CAddr(int x, int y);
    void IncC();
    std::uint16_t ExtRamAddress(std::uint32_t offset) const;
    std::uint8_t ExtRamRead(std::uint32_t offset) const;
    void ExtRamWrite(std::uint32_t offset, std::uint8_t value);
    void RenderTo(int line);
    void DrawLine(int screen_line);
    void FrameTick();
    void FinalizeFrame();
    void ClearLive();

    HardwareProfile profile_{};
    Tick tick_ = 0;
    Tick last_tick_ = 0;
    std::int64_t frame_index_ = 0;
    bool initialized_ = false;
    int cursor_line_ = 0;

    bool b_pin_ = false;
    std::uint8_t p1_ = 0;
    std::uint8_t mix_i8244_ = 0xFF;
    std::uint8_t mix_ef934x_ = 0xFF;

    std::uint8_t ta_ = 0;
    std::uint8_t tb_ = 0;
    bool busy_ = false;
    std::uint8_t x_ = 0;
    std::uint8_t y_ = 0;
    std::uint8_t y0_ = 0;
    std::uint8_t r_ = 0;
    std::uint8_t m_ = 0;
    bool blink_ = false;
    int blink_prescaler_ = 0;
    bool h_parity_ = false;

    std::uint8_t ram_a_[0x400]{};
    std::uint8_t ram_b_[0x400]{};
    std::uint8_t extram_[0x800]{};

    Framebuffer layer_{};     // live EF9340 layer (bit3 marker)
    Framebuffer composite_{}; // last completed composite frame
    std::uint64_t completed_frames_ = 0;
};

} // namespace o2em::ng
