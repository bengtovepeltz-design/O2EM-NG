#pragma once
// O2EM-NG Philips machine bus.
//
// The CPU sees only generic MCS-48 program/data/port/BUS operations. This
// class owns the Philips-specific decode: BIOS overlay, cartridge window,
// 128-byte external RAM, i824x chip select, P1/P2 routing, open bus, VDC IRQ
// and T0/T1 pins.
//
// Address ownership:
//   program 0x000-0x3FF  BIOS (MCS-48 internal ROM)
//   program 0x400-0xFFF  cartridge mapper
//   MOVX    0x00-0x7F    external 6810 RAM when P14=0 and P16=0
//   MOVX    any          i824x registers when P13=0 and P16=0
//   MOVX    any          cartridge I/O when P16=0
//   MOVX    unclaimed    0xFF open bus (deterministic; see open_bus.md)
//
// P1 hardware decode (active-low chip selects):
//   P10  cartridge bank select A
//   P11  cartridge bank select B
//   P12  keyboard/joystick matrix enable (low = enabled)
//   P13  i824x chip select (low = selected; read also needs P16 low)
//   P14  external RAM chip select (low = selected, only A7=0)
//   P15  Videopac+ VPP chip select (low = selected)
//   P16  external bus write enable (low = write enabled)
//   P17  luminance output (not a bus function)

#include <cstdint>
#include <vector>

#include "../cpu/mcs48_bus.h"
#include "../diagnostics/bus_trace.h"
#include "../input/input.h"
#include "../special/special_hardware.h"
#include "../timebase.h"
#include "../video/vdc.h"
#include "cartridge.h"
#include "mapper.h"

namespace o2em::ng {

class Vpp;

class MachineBus final : public Mcs48Bus {
public:
    MachineBus() = default;

    void Configure(const HardwareProfile& profile, const std::vector<std::uint8_t>* bios,
                   Cartridge* cartridge, Mapper* mapper, Vdc* vdc, Vpp* vpp,
                   MachineInput* input, BusTrace* trace,
                   SpecialHardware* special = nullptr);

    void PowerOn();
    void Reset();

    // Machine supplies the instruction-start tick and pre-instruction PC.
    void SetAccessContext(Tick tick, std::uint16_t pc) {
        tick_ = tick;
        pc_ = pc;
        if (vdc_) vdc_->SetTick(tick);
    }
    Tick AccessTick() const { return tick_; }

    // Mcs48Bus implementation.
    std::uint8_t ProgramRead(std::uint16_t address) override;
    std::uint8_t DataRead(std::uint8_t address) override;
    void DataWrite(std::uint8_t address, std::uint8_t value) override;
    std::uint8_t BusRead() override;
    void BusWrite(std::uint8_t value) override;
    std::uint8_t PortRead(int port) override;
    void PortWrite(int port, std::uint8_t value) override;
    std::uint8_t ExpanderRead(int port) override;
    void ExpanderWrite(int port, std::uint8_t value) override;
    void ProgWrite(bool level) override;
    void T0Clock(bool enabled) override;

    // Device lines.
    bool IrqLine() const;
    bool T0() const;
    bool T1() const;

    // Deterministic test/override pins. Overrides are machine input, not a
    // title-specific path.
    void SetT0Override(bool enabled, bool level);
    void SetT1Override(bool enabled, bool level);

    const std::uint8_t* ExternalRam() const { return external_ram_; }
    std::uint8_t* MutableExternalRam() { return external_ram_; }
    std::uint8_t P1() const { return p1_; }
    std::uint8_t P2() const { return p2_; }
    std::uint8_t BusOut() const { return bus_out_; }
    bool T0ClockEnabled() const { return t0_clock_; }
    std::uint8_t Expander(int port) const {
        return (port >= 4 && port <= 7) ? expander_[port] : 0xFF;
    }
    bool ProgLevel() const { return prog_level_; }

    BusDevice DecodeMovxDevice(std::uint8_t address, bool write) const;

    // Diagnostic-only line transition record (IRQ/T0/T1).
    void RecordLineTransition(BusOp op, std::uint8_t data);

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

private:
    void Trace(BusOp op, std::uint16_t address, std::uint8_t data,
               BusDevice device);

    HardwareProfile profile_{};
    const std::vector<std::uint8_t>* bios_ = nullptr;
    Cartridge* cart_ = nullptr;
    Mapper* mapper_ = nullptr;
    Vdc* vdc_ = nullptr;
    Vpp* vpp_ = nullptr;
    MachineInput* input_ = nullptr;
    BusTrace* trace_ = nullptr;
    SpecialHardware* special_ = nullptr;

    Tick tick_ = 0;
    std::uint16_t pc_ = 0;
    std::uint8_t external_ram_[0x80]{};
    std::uint8_t p1_ = 0xFF;
    std::uint8_t p2_ = 0xFF;
    std::uint8_t bus_out_ = 0xFF;
    std::uint8_t expander_[8]{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    bool t0_clock_ = false;
    bool prog_level_ = false;
    bool t0_override_enabled_ = false;
    bool t0_override_ = false;
    bool t1_override_enabled_ = false;
    bool t1_override_ = false;
};

} // namespace o2em::ng
