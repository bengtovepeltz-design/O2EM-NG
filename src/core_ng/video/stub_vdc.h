#pragma once
// O2EM-NG NG-2 stub i8244/i8245.
//
// This is deliberately NOT the real video chip. It models only the bus-visible
// registers and beam/timing status needed to execute real BIOS and cartridge
// code: A1 VBL/status IRQ, A2 collision clear, A4/A5 beam position, register
// write guards, and the VDC IRQ line.
//
// NG-3 replaces this with the full i8244/i8245 renderer. No rendering,
// palette or sprite work belongs here.

#include <cstdint>

#include "../state.h"
#include "../timebase.h"

namespace o2em::ng {

class StubVdc {
public:
    void Configure(const HardwareProfile& profile);
    void Reset();

    // Beam position and status are pure functions of the global tick. No
    // frame-local clock is stored or reset.
    void SetTick(Tick tick) { tick_ = tick; }

    void AssertVblankIrq();
    void ClearIrq();

    std::uint8_t Read(std::uint8_t reg);
    void Write(std::uint8_t reg, std::uint8_t value);

    bool IrqLine() const { return irq_line_; }
    bool Vblank() const;
    bool Hblank() const;

    int Hpos(Tick tick) const;
    int Vpos(Tick tick) const;
    int XBeam(Tick tick) const;
    int YBeam(Tick tick) const;

    std::uint8_t Control() const { return regs_[0xA0]; }
    std::uint8_t Status() const { return control_status_; }
    const std::uint8_t* Registers() const { return regs_; }

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

private:
    static std::uint8_t FixRegisterMirrors(std::uint8_t offset);

    HardwareProfile profile_{};
    Tick tick_ = 0;
    std::uint8_t regs_[256]{};
    std::uint8_t control_status_ = 0;
    std::uint8_t collision_status_ = 0;
    std::uint8_t x_latch_ = 0;
    std::uint8_t y_latch_ = 0;
    bool irq_line_ = false;
};

} // namespace o2em::ng
