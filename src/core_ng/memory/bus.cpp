#include "bus.h"
#include "../video/vpp.h"

#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace o2em::ng {

namespace {
constexpr std::uint8_t kOpenBus = 0xFF;
}

void MachineBus::Configure(const HardwareProfile& profile,
                           const std::vector<std::uint8_t>* bios,
                           Cartridge* cartridge, Mapper* mapper, Vdc* vdc, Vpp* vpp,
                           MachineInput* input, BusTrace* trace,
                           SpecialHardware* special) {
    profile_ = profile;
    bios_ = bios;
    cart_ = cartridge;
    mapper_ = mapper;
    vdc_ = vdc;
    vpp_ = vpp;
    input_ = input;
    trace_ = trace;
    special_ = special;
}

void MachineBus::PowerOn() {
    std::memset(external_ram_, 0, sizeof(external_ram_));
    p1_ = 0xFF;
    p2_ = 0xFF;
    bus_out_ = 0xFF;
    for (auto& e : expander_) e = 0xFF;
    t0_clock_ = false;
    prog_level_ = false;
    t0_override_enabled_ = false;
    t0_override_ = false;
    t1_override_enabled_ = false;
    t1_override_ = false;
    tick_ = 0;
    pc_ = 0;
}

void MachineBus::Reset() {
    // Hardware RESET puts the quasi-bidirectional ports back into input mode
    // (0xFF), matching MAME's i8048 reset. The 6810 external RAM is not
    // cleared by RESET; only power-on clears it.
    p1_ = 0xFF;
    p2_ = 0xFF;
    bus_out_ = 0xFF;
    t0_clock_ = false;
    prog_level_ = false;
    t0_override_enabled_ = false;
    t0_override_ = false;
    t1_override_enabled_ = false;
    t1_override_ = false;
}

std::uint8_t MachineBus::ProgramRead(std::uint16_t address) {
    std::uint8_t data = 0xFF;
    BusDevice device = BusDevice::OpenBus;

    if (address <= 0x03FF) {
        if (bios_ && address < bios_->size()) data = (*bios_)[address];
        device = BusDevice::Bios;
    } else if (address <= 0x0FFF) {
        if (mapper_) data = mapper_->ReadProgram(address);
        device = BusDevice::Cartridge;
    }

    Trace(BusOp::ProgramFetch, address, data, device);
    return data;
}

std::uint8_t MachineBus::DataRead(std::uint8_t address) {
    // A special device that drives the 8048 external bus owns the access.
    // The C7010 module uses the A7/A5-selected read window and must be
    // consulted before the normal cartridge/VDC/Plus responses, exactly as
    // on the hardware.
    if (special_) {
        std::uint8_t special_value = kOpenBus;
        if (special_->ExternalRead(address, special_value)) {
            Trace(BusOp::MovxRead, address, special_value,
                  BusDevice::Special);
            return special_value;
        }
    }

    // MAME odyssey2_state::io_read: the cartridge is on the external data
    // bus unconditionally; RAM and VDC additionally AND their outputs.
    std::uint8_t data = mapper_ ? mapper_->IoRead(address) : kOpenBus;
    BusDevice device = cart_ ? BusDevice::Cartridge : BusDevice::OpenBus;

    if (!(p1_ & 0x10) && !(address & 0x80)) {
        data &= external_ram_[address & 0x7F];
        device = BusDevice::ExtRam;
    }

    if ((p1_ & 0x48) == 0) {
        data &= vdc_->Read(address);
        device = BusDevice::Vdc;
    }

    if (vpp_ && !(p1_ & 0x20)) {
        data &= vpp_->Read(address);
        device = BusDevice::Vpp;
    }

    Trace(BusOp::MovxRead, address, data, device);
    return data;
}

void MachineBus::DataWrite(std::uint8_t address, std::uint8_t value) {
    BusDevice device = BusDevice::OpenBus;

    // The special device may claim the external write. It must still see the
    // VDC-selected case so the normal VDC write can proceed as on hardware.
    if (special_) {
        const bool handled = special_->ExternalWrite(address, value);
        if (handled && (p1_ & 0x08)) {
            Trace(BusOp::MovxWrite, address, value, BusDevice::Special);
            return;
        }
    }

    if (!(p1_ & 0x40)) {
        if (mapper_) mapper_->IoWrite(address, value);
        device = BusDevice::Cartridge;

        if (!(p1_ & 0x10) && !(address & 0x80)) {
            external_ram_[address & 0x7F] = value;
            device = BusDevice::ExtRam;
        }
    }

    if (!(p1_ & 0x08)) {
        vdc_->Write(address, value);
        device = BusDevice::Vdc;
    }

    if (vpp_ && !(p1_ & 0x20)) {
        vpp_->Write(address, value);
        device = BusDevice::Vpp;
    }

    Trace(BusOp::MovxWrite, address, value, device);
}

std::uint8_t MachineBus::BusRead() {
    static int trace = -1;
    static int lastP1 = -1;
    static int lastP2 = -1;
    if (trace < 0) {
        const char* env = std::getenv("O2EM_INPUT_TRACE");
        trace = (env && *env && *env != '0') ? 1 : 0;
    }
    if (trace && (p1_ != lastP1 || p2_ != lastP2)) {
        lastP1 = p1_;
        lastP2 = p2_;
        std::fprintf(stderr, "INPUT_BUSENTER p1=%02X p2=%02X select=%02X\n",
                     p1_, p2_, static_cast<unsigned>(p2_ & 0x07));
    }
    std::uint8_t data = mapper_ ? mapper_->BusRead() : kOpenBus;

    // The joystick matrix shares the keyboard decoder chip. P12 low enables
    // the matrix; P2[2:0] selects left (0) or right (1) controller.
    if (!(p1_ & 0x04) && input_) {
        const std::uint8_t select = static_cast<std::uint8_t>(p2_ & 0x07);
        if (select < 2) {
            const ControllerState& c = select == 0 ? input_->p1 : input_->p2;
            data &= static_cast<std::uint8_t>(~ControllerBits(c));
            static int trace = -1;
            static int last[2] = {-1, -1};
            if (trace < 0) {
                const char* env = std::getenv("O2EM_INPUT_TRACE");
                trace = (env && *env && *env != '0') ? 1 : 0;
            }
            if (trace && data != last[select]) {
                last[select] = data;
                std::fprintf(stderr, "INPUT_BUS p%d byte=%02X\n",
                             select + 1, data);
            }
        }
    }

    Trace(BusOp::BusRead, 0, data, BusDevice::Bus);
    return data;
}

void MachineBus::BusWrite(std::uint8_t value) {
    bus_out_ = value;
    if (mapper_) mapper_->BusWrite(value);
    Trace(BusOp::BusWrite, 0, value, BusDevice::Bus);
}

std::uint8_t MachineBus::PortRead(int port) {
    std::uint8_t data = 0xFF;

    if (port == 1) {
        // P1 is a quasi-bidirectional output port. With no external driver
        // the pin state follows the latch.
        data = p1_;
    } else if (port == 2) {
        // 74156 row decoder + 74148 priority encoder. P12 low enables it;
        // P2[2:0] selects the keyboard row. A pressed key pulls the encoder
        // output to (row position << 5), with the low nibble high.
        data = 0xFF;
        if (!(p1_ & 0x04) && input_) {
            const std::uint8_t row = input_->keyboard.rows[p2_ & 0x07];
            // std::countl_zero on a promoted unsigned counts 32 bits; the
            // 74148 sees an 8-bit row, so subtract the 24 high zero bits.
            const unsigned inp =
                row ? (std::countl_zero(static_cast<unsigned>(row)) - 24u) : 8u;
            if (inp < 8)
                data &= static_cast<std::uint8_t>((inp << 5) | 0x0F);
        }
    }

    Trace(BusOp::PortRead, static_cast<std::uint16_t>(port), data,
          BusDevice::Port);
    return data;
}

void MachineBus::PortWrite(int port, std::uint8_t value) {
    if (port == 1) {
        p1_ = value;
        if (vpp_) vpp_->SetP1(value);
        if (mapper_) mapper_->WriteP1(static_cast<std::uint8_t>(value & 0x13));
        if (special_) special_->WriteP1(value);
    } else if (port == 2) {
        p2_ = value;
        // MAME odyssey2_state::p2_write forwards the low nibble to the cart.
        if (mapper_) mapper_->WriteP2(static_cast<std::uint8_t>(value & 0x0F));
    }
    Trace(BusOp::PortWrite, static_cast<std::uint16_t>(port), value,
          BusDevice::Port);
}

std::uint8_t MachineBus::ExpanderRead(int port) {
    if (port < 4 || port > 7) return 0xFF;
    return expander_[port];
}

void MachineBus::ExpanderWrite(int port, std::uint8_t value) {
    if (port >= 4 && port <= 7) {
        expander_[port] = value;
        if (vpp_) vpp_->ExpanderWrite(port, value);
    }
}

void MachineBus::ProgWrite(bool level) { prog_level_ = level; }

void MachineBus::T0Clock(bool enabled) { t0_clock_ = enabled; }

bool MachineBus::IrqLine() const {
    return vdc_ ? vdc_->IrqLine() : false;
}

bool MachineBus::T0() const {
    if (t0_override_enabled_) return t0_override_;
    return mapper_ ? mapper_->T0() : false;
}

bool MachineBus::T1() const {
    if (t1_override_enabled_) return t1_override_;
    if (!vdc_) return false;
    return vdc_->Vblank() || vdc_->Hblank();
}

void MachineBus::SetT0Override(bool enabled, bool level) {
    t0_override_enabled_ = enabled;
    t0_override_ = level;
}

void MachineBus::SetT1Override(bool enabled, bool level) {
    t1_override_enabled_ = enabled;
    t1_override_ = level;
}

BusDevice MachineBus::DecodeMovxDevice(std::uint8_t address, bool) const {
    if ((p1_ & 0x48) == 0) return BusDevice::Vdc;
    if (!(p1_ & 0x10) && !(address & 0x80)) return BusDevice::ExtRam;
    if (!(p1_ & 0x40)) return BusDevice::Cartridge;
    return BusDevice::OpenBus;
}

void MachineBus::RecordLineTransition(BusOp op, std::uint8_t data) {
    Trace(op, 0, data, BusDevice::Cpu);
}

void MachineBus::Trace(BusOp op, std::uint16_t address, std::uint8_t data,
                       BusDevice device) {
    if (!trace_ || !trace_->Enabled()) return;
    BusRecord r;
    r.tick = tick_;
    r.cpu_cycle = static_cast<std::uint64_t>(tick_ / kTicksPerCpuCycle);
    r.pc = pc_;
    r.op = op;
    r.address = address;
    r.data = data;
    r.p1 = p1_;
    r.p2 = p2_;
    r.bank = mapper_ ? mapper_->Bank() : 0;
    r.device = device;
    trace_->Record(r);
}

bool MachineBus::SaveState(SnapshotWriter& w) const {
    w.u32(0x42555331u); // "BUS1"
    w.u64(static_cast<std::uint64_t>(tick_));
    w.u16(pc_);
    w.raw(external_ram_, sizeof(external_ram_));
    w.u8(p1_);
    w.u8(p2_);
    w.u8(bus_out_);
    w.raw(expander_, sizeof(expander_));
    w.u8(t0_clock_ ? 1 : 0);
    w.u8(prog_level_ ? 1 : 0);
    w.u8(t0_override_enabled_ ? 1 : 0);
    w.u8(t0_override_ ? 1 : 0);
    w.u8(t1_override_enabled_ ? 1 : 0);
    w.u8(t1_override_ ? 1 : 0);
    return true;
}

bool MachineBus::LoadState(SnapshotReader& r) {
    if (r.u32() != 0x42555331u) return false;
    tick_ = static_cast<Tick>(r.u64());
    pc_ = r.u16();
    r.raw(external_ram_, sizeof(external_ram_));
    p1_ = r.u8();
    p2_ = r.u8();
    bus_out_ = r.u8();
    r.raw(expander_, sizeof(expander_));
    t0_clock_ = r.u8() != 0;
    prog_level_ = r.u8() != 0;
    t0_override_enabled_ = r.u8() != 0;
    t0_override_ = r.u8() != 0;
    t1_override_enabled_ = r.u8() != 0;
    t1_override_ = r.u8() != 0;
    return r.ok;
}

} // namespace o2em::ng
