// O2EM-NG MCS48-NG production CPU.
//
// Independent owned-state Intel 8048 implementation for the new machine core.
// Behavioral reference: Intel MCS-48 documentation and MAME 0.289 mcs48
// (BSD-3-Clause). See Docs/Development/NG-1_MCS48_NG.md for provenance.
//
// No legacy cpu.cpp state or globals. No SDL/frontend/VDC knowledge.

#include "mcs48_ng.h"

#include <cstring>
#include <type_traits>
#include <utility>

namespace o2em::ng {

namespace {

constexpr std::uint8_t C_FLAG = 0x80;
constexpr std::uint8_t A_FLAG = 0x40;
constexpr std::uint8_t F0_FLAG = 0x20;
constexpr std::uint8_t B_FLAG = 0x10;

inline std::uint8_t PswValue(const Mcs48State& s) {
    return static_cast<std::uint8_t>((s.psw & 0xF8) | 0x08 | (s.sp & 0x07));
}

inline void UpdateBank(Mcs48State& s) {
    s.reg_bank = (s.psw & B_FLAG) ? 1 : 0;
}

inline std::uint8_t& Reg(Mcs48State& s, int n) {
    return s.ram[(s.reg_bank ? 24 : 0) + n];
}

inline std::uint8_t& Ram(Mcs48State& s, std::uint8_t address) {
    return s.ram[address & 0x3F];
}

} // namespace

void Mcs48Cpu::PowerOn() {
    std::memset(&state_, 0, sizeof(state_));
    Reset();
    state_.cycles = 0;
}

void Mcs48Cpu::Reset() {
    state_.a = 0;
    state_.pc = 0;
    state_.psw = 0;
    state_.sp = 0;
    state_.reg_bank = 0;
    state_.a11 = 0;
    state_.timer = 0;
    state_.prescaler = 0;
    state_.timer_mode = 0;
    state_.timer_flag = 0;
    state_.t1_history = 0;
    // RESET puts the quasi-bidirectional ports into input mode (0xFF),
    // matching the MCS-48 reset description and MAME 0.289 mcs48.cpp.
    state_.p1 = 0xFF;
    state_.p2 = 0xFF;
    state_.bus_latch = 0;
    state_.f0 = false;
    state_.f1 = false;
    state_.t0 = false;
    state_.t1 = false;
    state_.irq_enabled = false;
    state_.irq_pending = false;
    state_.irq_in_service = false;
    state_.timer_irq_enabled = false;
    state_.timer_irq_pending = false;
    state_.last_pc = 0;
    state_.illegal_opcode = 0;
    state_.illegal_count = 0;
    // RAM is deliberately preserved across RESET (hardware semantics).
}

void Mcs48Cpu::SetIrqLevel(bool asserted) {
    state_.irq_pending = asserted;
}

void Mcs48Cpu::AcknowledgeIrq() {
    state_.irq_pending = false;
}

void Mcs48Cpu::SetT0(bool level) { state_.t0 = level; }
void Mcs48Cpu::SetT1(bool level) { state_.t1 = level; }

void Mcs48Cpu::SaveState(std::vector<std::uint8_t>& out) const {
    static_assert(std::is_trivially_copyable<Mcs48State>::value,
                  "Mcs48State must remain POD for deterministic snapshots");
    out.resize(sizeof(Mcs48State));
    std::memcpy(out.data(), &state_, sizeof(Mcs48State));
}

bool Mcs48Cpu::LoadState(const std::uint8_t* data, std::size_t size) {
    if (!data || size != sizeof(Mcs48State))
        return false;
    std::memcpy(&state_, data, sizeof(Mcs48State));
    return true;
}

void Mcs48Cpu::Burn(std::uint32_t count) {
    if (count == 0)
        return;

    if (state_.timer_mode & 0x01) {
        const std::uint8_t old = state_.timer;
        const std::uint16_t prescaler =
            static_cast<std::uint16_t>(state_.prescaler) + count;
        state_.timer = static_cast<std::uint8_t>((state_.timer + (prescaler >> 5)) & 0xFF);
        state_.prescaler = static_cast<std::uint8_t>(prescaler & 0x1F);
        if (state_.timer < old)
            TimerOverflow();
    } else if (state_.timer_mode & 0x02) {
        for (std::uint32_t i = 0; i < count; ++i) {
            state_.t1_history = static_cast<std::uint8_t>(
                (state_.t1_history << 1) | (state_.t1 ? 1 : 0));
            if ((state_.t1_history & 0x03) == 0x02) {
                if (++state_.timer == 0)
                    TimerOverflow();
            }
        }
    }
    state_.cycles += count;
}

void Mcs48Cpu::TimerOverflow() {
    state_.timer_flag = 1;
    if (state_.timer_irq_enabled)
        state_.timer_irq_pending = true;
}

bool Mcs48Cpu::CheckIrqs() {
    if (state_.irq_in_service)
        return false;

    if (state_.irq_pending && state_.irq_enabled) {
        Burn(2);
        state_.irq_in_service = true;
        Call(0x003);
        return true;
    }
    if (state_.timer_irq_pending && state_.timer_irq_enabled) {
        Burn(2);
        state_.irq_in_service = true;
        Call(0x007);
        state_.timer_irq_pending = false;
        return true;
    }
    return false;
}

std::uint8_t Mcs48Cpu::Fetch() {
    const std::uint16_t address = static_cast<std::uint16_t>(state_.pc & 0x0FFF);
    const std::uint8_t value = bus_.ProgramRead(address);
    state_.pc = static_cast<std::uint16_t>(
        ((state_.pc + 1) & 0x07FF) | (state_.pc & 0x0800));
    return value;
}

void Mcs48Cpu::Push() {
    const std::uint8_t stack = state_.sp & 0x07;
    state_.ram[8 + 2 * stack] = static_cast<std::uint8_t>(state_.pc & 0xFF);
    state_.ram[9 + 2 * stack] = static_cast<std::uint8_t>(
        ((state_.pc >> 8) & 0x0F) | (PswValue(state_) & 0xF0));
    state_.sp = static_cast<std::uint8_t>((stack + 1) & 0x07);
}

void Mcs48Cpu::PopPc() {
    state_.sp = static_cast<std::uint8_t>((state_.sp - 1) & 0x07);
    const std::uint8_t low = state_.ram[8 + 2 * state_.sp];
    const std::uint8_t high = state_.ram[9 + 2 * state_.sp];
    std::uint16_t target = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high & 0x0F) << 8) | low);
    if (state_.irq_in_service)
        target &= 0x07FF; // MAME/hardware: RET during service stays in bank 0
    state_.pc = target;
}

void Mcs48Cpu::PopPcPsw() {
    state_.sp = static_cast<std::uint8_t>((state_.sp - 1) & 0x07);
    const std::uint8_t low = state_.ram[8 + 2 * state_.sp];
    const std::uint8_t high = state_.ram[9 + 2 * state_.sp];
    std::uint16_t target = static_cast<std::uint16_t>(
        (static_cast<std::uint16_t>(high & 0x0F) << 8) | low);
    state_.psw = static_cast<std::uint8_t>(high & 0xF0);
    UpdateBank(state_);
    state_.pc = target;
}

void Mcs48Cpu::Call(std::uint16_t target) {
    Push();
    Jump(target);
}

void Mcs48Cpu::Jump(std::uint16_t target) {
    const std::uint16_t bank = state_.irq_in_service ? 0 : (state_.a11 & 0x0800);
    state_.pc = static_cast<std::uint16_t>((target & 0x07FF) | bank);
}

void Mcs48Cpu::Branch(bool condition) {
    const std::uint8_t offset = Fetch();
    if (condition)
        state_.pc = static_cast<std::uint16_t>((state_.pc & 0x0F00) | offset);
}

void Mcs48Cpu::JumpPage(int page) {
    const std::uint8_t offset = Fetch();
    Jump(static_cast<std::uint16_t>((page << 8) | offset));
}

void Mcs48Cpu::CallPage(int page) {
    const std::uint8_t offset = Fetch();
    Call(static_cast<std::uint16_t>((page << 8) | offset));
}

void Mcs48Cpu::Add(std::uint8_t value, bool carryIn) {
    const std::uint8_t carry = carryIn ? 1 : 0;
    const std::uint16_t sum = static_cast<std::uint16_t>(
        state_.a + value + carry);
    const std::uint8_t ac = static_cast<std::uint8_t>(
        ((state_.a & 0x0F) + (value & 0x0F) + carry) > 0x0F ? A_FLAG : 0);
    state_.psw = static_cast<std::uint8_t>(state_.psw & ~(C_FLAG | A_FLAG));
    if (sum > 0xFF)
        state_.psw |= C_FLAG;
    state_.psw |= ac;
    state_.a = static_cast<std::uint8_t>(sum & 0xFF);
}

void Mcs48Cpu::Da() {
    if ((state_.a & 0x0F) > 0x09 || (state_.psw & A_FLAG)) {
        if (state_.a > 0xF9)
            state_.psw |= C_FLAG;
        state_.a = static_cast<std::uint8_t>(state_.a + 0x06);
    }
    if ((state_.a & 0xF0) > 0x90 || (state_.psw & C_FLAG)) {
        state_.a = static_cast<std::uint8_t>(state_.a + 0x60);
        state_.psw |= C_FLAG;
    }
}

std::uint32_t Mcs48Cpu::Execute(std::uint8_t op) {
    std::uint32_t cycles = 1;

    // Register/accumulator ranges first (cheap and complete).
    if (op >= 0x18 && op <= 0x1F) { ++Reg(state_, op - 0x18); return 1; }
    if (op >= 0x28 && op <= 0x2F) {
        const int r = op - 0x28;
        std::swap(state_.a, Reg(state_, r));
        return 1;
    }
    if (op >= 0x48 && op <= 0x4F) { state_.a |= Reg(state_, op - 0x48); return 1; }
    if (op >= 0x58 && op <= 0x5F) { state_.a &= Reg(state_, op - 0x58); return 1; }
    if (op >= 0x68 && op <= 0x6F) { Add(Reg(state_, op - 0x68), false); return 1; }
    if (op >= 0x78 && op <= 0x7F) {
        Add(Reg(state_, op - 0x78), (state_.psw & C_FLAG) != 0);
        return 1;
    }
    if (op >= 0xA8 && op <= 0xAF) { Reg(state_, op - 0xA8) = state_.a; return 1; }
    if (op >= 0xB8 && op <= 0xBF) { Reg(state_, op - 0xB8) = Fetch(); return 2; }
    if (op >= 0xC8 && op <= 0xCF) { --Reg(state_, op - 0xC8); return 1; }
    if (op >= 0xD8 && op <= 0xDF) { state_.a ^= Reg(state_, op - 0xD8); return 1; }
    if (op >= 0xE8 && op <= 0xEF) {
        std::uint8_t& r = Reg(state_, op - 0xE8);
        const std::uint8_t offset = Fetch();
        if (--r != 0)
            state_.pc = static_cast<std::uint16_t>((state_.pc & 0x0F00) | offset);
        return 2;
    }
    if (op >= 0xF8 && op <= 0xFF) { state_.a = Reg(state_, op - 0xF8); return 1; }

    switch (op) {
    case 0x00: return 1; // NOP
    case 0x02: // OUTL BUS,A
        state_.bus_latch = state_.a;
        bus_.BusWrite(state_.a);
        return 2;
    case 0x03: Add(Fetch(), false); return 2; // ADD A,#n
    case 0x04: JumpPage(0); return 2;
    case 0x05: state_.irq_enabled = true; return 1; // EN I
    case 0x06: break; // illegal
    case 0x07: --state_.a; return 1; // DEC A
    case 0x08: state_.a = bus_.BusRead(); return 2; // INS A,BUS
    case 0x09: state_.a = static_cast<std::uint8_t>(bus_.PortRead(1) & state_.p1); return 2;
    case 0x0A: state_.a = static_cast<std::uint8_t>(bus_.PortRead(2) & state_.p2); return 2;
    case 0x0B: break; // illegal
    case 0x0C: case 0x0D: case 0x0E: case 0x0F:
        state_.a = static_cast<std::uint8_t>(bus_.ExpanderRead((op - 0x0C) + 4) & 0x0F);
        return 2;
    case 0x10: ++Ram(state_, Reg(state_, 0)); return 1;
    case 0x11: ++Ram(state_, Reg(state_, 1)); return 1;
    case 0x12: Branch((state_.a & 0x01) != 0); return 2;
    case 0x13: Add(Fetch(), (state_.psw & C_FLAG) != 0); return 2;
    case 0x14: CallPage(0); return 2;
    case 0x15: state_.irq_enabled = false; return 1; // DIS I
    case 0x16: // JTF
        {
            const bool taken = state_.timer_flag != 0;
            const std::uint8_t offset = Fetch();
            if (taken)
                state_.pc = static_cast<std::uint16_t>((state_.pc & 0x0F00) | offset);
            state_.timer_flag = 0;
        }
        return 2;
    case 0x17: ++state_.a; return 1;
    case 0x20: std::swap(state_.a, Ram(state_, Reg(state_, 0))); return 1;
    case 0x21: std::swap(state_.a, Ram(state_, Reg(state_, 1))); return 1;
    case 0x22: break; // illegal
    case 0x23: state_.a = Fetch(); return 2;
    case 0x24: JumpPage(1); return 2;
    case 0x25: state_.timer_irq_enabled = true; return 1; // EN TCNTI
    case 0x26: Branch(!state_.t0); return 2; // JNT0
    case 0x27: state_.a = 0; return 1; // CLR A
    case 0x30: { // XCHD A,@R0
        std::uint8_t& m = Ram(state_, Reg(state_, 0));
        const std::uint8_t t = state_.a;
        state_.a = static_cast<std::uint8_t>((state_.a & 0xF0) | (m & 0x0F));
        m = static_cast<std::uint8_t>((m & 0xF0) | (t & 0x0F));
        return 1;
    }
    case 0x31: {
        std::uint8_t& m = Ram(state_, Reg(state_, 1));
        const std::uint8_t t = state_.a;
        state_.a = static_cast<std::uint8_t>((state_.a & 0xF0) | (m & 0x0F));
        m = static_cast<std::uint8_t>((m & 0xF0) | (t & 0x0F));
        return 1;
    }
    case 0x32: Branch((state_.a & 0x02) != 0); return 2;
    case 0x33: break; // illegal
    case 0x34: CallPage(1); return 2;
    case 0x35: // DIS TCNTI
        state_.timer_irq_enabled = false;
        state_.timer_irq_pending = false;
        return 1;
    case 0x36: Branch(state_.t0); return 2; // JT0
    case 0x37: state_.a = static_cast<std::uint8_t>(~state_.a); return 1;
    case 0x39: state_.p1 = state_.a; bus_.PortWrite(1, state_.p1); return 2;
    case 0x3A: state_.p2 = state_.a; bus_.PortWrite(2, state_.p2); return 2;
    case 0x3B: break; // illegal
    case 0x3C: case 0x3D: case 0x3E: case 0x3F:
        bus_.ExpanderWrite((op - 0x3C) + 4, static_cast<std::uint8_t>(state_.a & 0x0F));
        return 2;
    case 0x40: state_.a |= Ram(state_, Reg(state_, 0)); return 1;
    case 0x41: state_.a |= Ram(state_, Reg(state_, 1)); return 1;
    case 0x42: state_.a = state_.timer; return 1; // MOV A,T
    case 0x43: state_.a |= Fetch(); return 2;
    case 0x44: JumpPage(2); return 2;
    case 0x45: // STRT CNT
        if (!(state_.timer_mode & 0x02))
            state_.t1_history = state_.t1 ? 1 : 0;
        state_.timer_mode = 0x02;
        return 1;
    case 0x46: Branch(!state_.t1); return 2; // JNT1
    case 0x47: state_.a = static_cast<std::uint8_t>((state_.a << 4) | (state_.a >> 4)); return 1;
    case 0x50: state_.a &= Ram(state_, Reg(state_, 0)); return 1;
    case 0x51: state_.a &= Ram(state_, Reg(state_, 1)); return 1;
    case 0x52: Branch((state_.a & 0x04) != 0); return 2;
    case 0x53: state_.a &= Fetch(); return 2;
    case 0x54: CallPage(2); return 2;
    case 0x55: // STRT T
        state_.timer_mode = 0x01;
        state_.prescaler = 0;
        return 1;
    case 0x56: Branch(state_.t1); return 2; // JT1
    case 0x57: Da(); return 1;
    case 0x60: Add(Ram(state_, Reg(state_, 0)), false); return 1;
    case 0x61: Add(Ram(state_, Reg(state_, 1)), false); return 1;
    case 0x62: state_.timer = state_.a; return 1; // MOV T,A
    case 0x63: break; // illegal
    case 0x64: JumpPage(3); return 2;
    case 0x65: state_.timer_mode = 0; return 1; // STOP TCNT
    case 0x66: break; // illegal
    case 0x67: { // RRC A
        const std::uint8_t newC = (state_.a & 0x01) ? 0x80 : 0;
        state_.a = static_cast<std::uint8_t>((state_.a >> 1) | ((state_.psw & C_FLAG) ? 0x80 : 0));
        if (newC) state_.psw |= C_FLAG; else state_.psw &= ~C_FLAG;
        return 1;
    }
    case 0x70: Add(Ram(state_, Reg(state_, 0)), (state_.psw & C_FLAG) != 0); return 1;
    case 0x71: Add(Ram(state_, Reg(state_, 1)), (state_.psw & C_FLAG) != 0); return 1;
    case 0x72: Branch((state_.a & 0x08) != 0); return 2;
    case 0x73: break; // illegal
    case 0x74: CallPage(3); return 2;
    case 0x75: bus_.T0Clock(true); return 1; // ENT0 CLK
    case 0x76: Branch(state_.f1); return 2;
    case 0x77: state_.a = static_cast<std::uint8_t>((state_.a >> 1) | (state_.a << 7)); return 1;
    case 0x80: state_.a = bus_.DataRead(Reg(state_, 0)); return 2;
    case 0x81: state_.a = bus_.DataRead(Reg(state_, 1)); return 2;
    case 0x82: break; // illegal
    case 0x83: PopPc(); return 2; // RET
    case 0x84: JumpPage(4); return 2;
    case 0x85: state_.f0 = false; return 1;
    case 0x86: Branch(state_.irq_pending); return 2; // JNI
    case 0x87: break; // illegal
    case 0x88: { // ORL BUS,#n
        const std::uint8_t operand = Fetch();
        bus_.BusWrite(static_cast<std::uint8_t>(bus_.BusRead() | operand));
        return 2;
    }
    case 0x89: state_.p1 = static_cast<std::uint8_t>(state_.p1 | Fetch()); bus_.PortWrite(1, state_.p1); return 2;
    case 0x8A: state_.p2 = static_cast<std::uint8_t>(state_.p2 | Fetch()); bus_.PortWrite(2, state_.p2); return 2;
    case 0x8B: break; // illegal
    case 0x8C: case 0x8D: case 0x8E: case 0x8F: {
        const int port = (op - 0x8C) + 4;
        bus_.ExpanderWrite(port, static_cast<std::uint8_t>(
            (bus_.ExpanderRead(port) & 0x0F) | (state_.a & 0x0F)));
        return 2;
    }
    case 0x90: bus_.DataWrite(Reg(state_, 0), state_.a); return 2;
    case 0x91: bus_.DataWrite(Reg(state_, 1), state_.a); return 2;
    case 0x92: Branch((state_.a & 0x10) != 0); return 2;
    case 0x93: state_.irq_in_service = false; PopPcPsw(); return 2; // RETR
    case 0x94: CallPage(4); return 2;
    case 0x95: state_.f0 = !state_.f0; return 1;
    case 0x96: Branch(state_.a != 0); return 2; // JNZ
    case 0x97: state_.psw &= ~C_FLAG; return 1;
    case 0x98: { // ANL BUS,#n
        const std::uint8_t operand = Fetch();
        bus_.BusWrite(static_cast<std::uint8_t>(bus_.BusRead() & operand));
        return 2;
    }
    case 0x99: state_.p1 = static_cast<std::uint8_t>(state_.p1 & Fetch()); bus_.PortWrite(1, state_.p1); return 2;
    case 0x9A: state_.p2 = static_cast<std::uint8_t>(state_.p2 & Fetch()); bus_.PortWrite(2, state_.p2); return 2;
    case 0x9B: break; // illegal
    case 0x9C: case 0x9D: case 0x9E: case 0x9F: {
        const int port = (op - 0x9C) + 4;
        bus_.ExpanderWrite(port, static_cast<std::uint8_t>(
            (bus_.ExpanderRead(port) & 0x0F) & (state_.a & 0x0F)));
        return 2;
    }
    case 0xA0: Ram(state_, Reg(state_, 0)) = state_.a; return 1;
    case 0xA1: Ram(state_, Reg(state_, 1)) = state_.a; return 1;
    case 0xA2: break; // illegal
    case 0xA3: // MOVP A,@A
        state_.a = bus_.ProgramRead(static_cast<std::uint16_t>((state_.pc & 0x0F00) | state_.a));
        return 2;
    case 0xA4: JumpPage(5); return 2;
    case 0xA5: state_.f1 = false; return 1;
    case 0xA7: state_.psw ^= C_FLAG; return 1;
    case 0xB0: Ram(state_, Reg(state_, 0)) = Fetch(); return 2;
    case 0xB1: Ram(state_, Reg(state_, 1)) = Fetch(); return 2;
    case 0xB2: Branch((state_.a & 0x20) != 0); return 2;
    case 0xB3: // JMPP @A
        state_.pc = static_cast<std::uint16_t>(
            (state_.pc & 0x0F00) |
            bus_.ProgramRead(static_cast<std::uint16_t>((state_.pc & 0x0F00) | state_.a)));
        return 2;
    case 0xB4: CallPage(5); return 2;
    case 0xB5: state_.f1 = !state_.f1; return 1;
    case 0xB6: Branch(state_.f0); return 2;
    case 0xB7: break; // illegal
    case 0xC4: JumpPage(6); return 2;
    case 0xC5: state_.psw &= ~B_FLAG; UpdateBank(state_); return 1;
    case 0xC6: Branch(state_.a == 0); return 2; // JZ
    case 0xC7: state_.a = PswValue(state_); return 1;
    case 0xD0: state_.a ^= Ram(state_, Reg(state_, 0)); return 1;
    case 0xD1: state_.a ^= Ram(state_, Reg(state_, 1)); return 1;
    case 0xD2: Branch((state_.a & 0x40) != 0); return 2;
    case 0xD3: state_.a ^= Fetch(); return 2;
    case 0xD4: CallPage(6); return 2;
    case 0xD5: state_.psw |= B_FLAG; UpdateBank(state_); return 1;
    case 0xD6: break; // illegal
    case 0xD7:
        state_.psw = static_cast<std::uint8_t>(state_.a & 0xF8);
        state_.sp = static_cast<std::uint8_t>(state_.a & 0x07);
        UpdateBank(state_);
        return 1;
    case 0xE3: // MOVP3 A,@A
        state_.a = bus_.ProgramRead(static_cast<std::uint16_t>(0x0300 | state_.a));
        return 2;
    case 0xE4: JumpPage(7); return 2;
    case 0xE5: state_.a11 = 0x000; return 1;
    case 0xE6: Branch((state_.psw & C_FLAG) == 0); return 2; // JNC
    case 0xE7: state_.a = static_cast<std::uint8_t>((state_.a << 1) | (state_.a >> 7)); return 1;
    case 0xF0: state_.a = Ram(state_, Reg(state_, 0)); return 1;
    case 0xF1: state_.a = Ram(state_, Reg(state_, 1)); return 1;
    case 0xF2: Branch((state_.a & 0x80) != 0); return 2;
    case 0xF3: break; // illegal
    case 0xF4: CallPage(7); return 2;
    case 0xF5: state_.a11 = 0x800; return 1;
    case 0xF6: Branch((state_.psw & C_FLAG) != 0); return 2; // JC
    case 0xF7: { // RLC A
        const std::uint8_t newC = (state_.a & 0x80) ? 0x80 : 0;
        state_.a = static_cast<std::uint8_t>((state_.a << 1) | ((state_.psw & C_FLAG) ? 1 : 0));
        if (newC) state_.psw |= C_FLAG; else state_.psw &= ~C_FLAG;
        return 1;
    }
    default: break;
    }

    // Explicit illegal-opcode policy: consume one cycle, record, do not alias NOP.
    state_.illegal_opcode = op;
    ++state_.illegal_count;
    return 1;
}

StepResult Mcs48Cpu::Step() {
    StepResult result{};

    const bool irq = CheckIrqs();
    if (irq) {
        result.cycles = 2;
        result.irq_taken = true;
    }

    state_.last_pc = state_.pc;
    const std::uint8_t opcode = Fetch();
    const std::uint32_t cycles = Execute(opcode);
    Burn(cycles);
    result.cycles += cycles;
    return result;
}

} // namespace o2em::ng
