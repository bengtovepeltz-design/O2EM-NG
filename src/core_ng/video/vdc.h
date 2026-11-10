#pragma once
// O2EM-NG real Intel i8244 (NTSC) / i8245 (PAL) video and sound device.
//
// The device owns every piece of mutable VDC state: the 256-byte register
// file, the latched A1 status, the A2 collision status and mask, the A4/A5
// beam latches, the live beam-derived timing, the frame-global collision and
// priority maps, the 360x243 indexed framebuffer, the sound shift register,
// and the raster cursor.
//
// One timebase: every beam/blanking/IRQ value is a pure function of the
// authoritative NG Tick. There is no frame-local clock, no h_clk and no
// reset-to-zero beam timer. The machine advances the device with
// AdvanceTo(Tick); register writes are applied only after the raster has been
// advanced to the write's beam position (update-on-write).
//
// Behavioural reference: MAME 0.289 i8244/i8245 (BSD-3-Clause). The
// implementation is an independent C++ port of the documented semantics, not a
// source copy; see Docs/Development/NG-0_Provenance.md.

#include <cstdint>

#include "../state.h"
#include "../timebase.h"
#include "framebuffer.h"
#include "vdc_state.h"

namespace o2em::ng {

class Vpp;
class AudioDevice;

// NG-0 device contract. Kept so later VPP/EF934x work can compose layers.
class VdcDevice {
public:
    virtual ~VdcDevice() = default;

    virtual void Reset() = 0;
    virtual void AdvanceTo(Tick tick) = 0;

    virtual std::uint8_t Read(std::uint8_t reg) = 0;
    virtual void Write(std::uint8_t reg, std::uint8_t value) = 0;

    virtual bool IrqLine() const = 0;
    virtual const Framebuffer& Framebuffer() const = 0;

    virtual void Snapshot(SnapshotWriter&) const = 0;
    virtual bool Restore(SnapshotReader&) = 0;
};

class Vdc final : public VdcDevice {
public:
    void Configure(const HardwareProfile& profile);

    void Reset() override;
    void AdvanceTo(Tick tick) override;

    // The machine supplies the current instruction-start tick before every
    // bus access. SetTick only records it; AdvanceTo performs the raster work.
    void SetTick(Tick tick) { tick_ = tick; }
    Tick CurrentTick() const { return tick_; }

    std::uint8_t Read(std::uint8_t reg) override;
    void Write(std::uint8_t reg, std::uint8_t value) override;

    // VBL event from the machine scheduler (same authoritative tick/profile).
    void AssertVblankIrq();
    void ClearIrq();
    bool IrqLine() const override;

    // Live hardware signals (pure functions of tick_).
    bool Vblank() const;
    bool Hblank() const;
    int Hpos(Tick tick) const;
    int Vpos(Tick tick) const;
    int XBeam(Tick tick) const;
    int YBeam(Tick tick) const;

    std::uint8_t Control() const { return regs_[kRegControl]; }
    std::uint8_t Status() const;
    const std::uint8_t* Registers() const { return regs_; }

    // Presented frame = last completed visible frame. LiveFramebuffer() is the
    // partially rendered current frame (useful for mid-frame tests).
    const o2em::ng::Framebuffer& Framebuffer() const override { return presented_; }
    const o2em::ng::Framebuffer& LiveFramebuffer() const { return live_; }
    std::uint64_t CompletedFrames() const { return completed_frames_; }
    std::uint64_t FrameNumber() const { return presented_.frame_number; }

    // External collision input (CX pin). The G7400 VPP asserts it per pixel.
    void ExternalCollision(int x, bool asserted = true);

    // G7400 composition: the VPP composites each completed scanline into the
    // live frame through this pointer.
    void AttachVpp(Vpp* vpp) { vpp_ = vpp; }
    void AttachAudio(AudioDevice* audio) { audio_ = audio; }
    std::uint8_t* LiveRow(int line);

    // 1-bit sound shift-register output (audio host work is deferred).
    bool SoundBit() const;

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

    void Snapshot(SnapshotWriter& w) const override { SaveState(w); }
    bool Restore(SnapshotReader& r) override { return LoadState(r); }

private:
    static std::uint8_t FixRegisterMirrors(std::uint8_t offset);
    bool IsNtsc() const { return profile_.vtotal == 263; }

    void RenderTo(int line, int x);
    void RenderSegment(int line, int x0, int x1);
    void RenderGrid(int line, int x0, int x1);
    void RenderMajor(int line, int x0, int x1);
    void RenderMinor(int line, int x0, int x1);
    void MajorPixel(std::uint8_t index, int x, int line, std::uint8_t pixel,
                    std::uint8_t color, int x0, int x1);
    void EdgePass(int line);

    std::uint8_t BackgroundColor() const;
    std::uint8_t GridColor() const;
    static std::uint8_t MajorColor(std::uint8_t color);
    static std::uint8_t MinorColor(std::uint8_t color);

    void ClockSound();
    void FinalizeFrame();
    void ClearLiveFrame();
    void ClearCollisionMaps();

    HardwareProfile profile_{};
    Vpp* vpp_ = nullptr;
    AudioDevice* audio_ = nullptr;
    Tick tick_ = 0;

    std::uint8_t regs_[kVdcRegisterCount]{};
    std::uint8_t control_status_ = 0;
    std::uint8_t collision_status_ = 0;
    std::uint8_t x_latch_ = 0;
    std::uint8_t y_latch_ = 0;
    bool irq_line_ = false;

    std::uint16_t collision_map_[kVdcCollisionMapSize]{};
    std::uint8_t priority_map_[kVdcCollisionMapSize]{};

    o2em::ng::Framebuffer live_{};
    o2em::ng::Framebuffer presented_{};
    std::uint64_t completed_frames_ = 0;

    // Raster cursor within the live frame. line 0..visible_h, x 0..visible_w.
    int cursor_line_ = 0;
    int cursor_x_ = 0;
    bool initialized_ = false;
    Tick last_tick_ = 0;
    std::int64_t frame_index_ = 0;

    // Sound shift register (hardware behavior; audio output is deferred).
    bool sh_written_ = false;
    bool sh_pending_ = false;
    std::uint8_t sh_prescaler_ = 0;
    std::uint8_t sh_count_ = 0;
    int sound_output_ = 0;
    std::uint8_t sh_duty_ = 0;
    Tick next_hblank_tick_ = 0;
};

} // namespace o2em::ng
