#pragma once
// NG-0 owned MCS48-NG state. No globals, no SDL, snapshot-complete.

#include <cstdint>

namespace o2em::ng {

struct Mcs48State {
    std::uint8_t a = 0;
    std::uint16_t pc = 0;          // 12-bit
    std::uint8_t psw = 0;
    std::uint8_t sp = 0;           // 3-bit stack pointer in PSW bits
    std::uint8_t ram[64] = {};     // internal RAM including register banks
    std::uint8_t reg_bank = 0;     // 0 or 1
    std::uint16_t a11 = 0;         // MB0/MB1 program-memory latch

    std::uint8_t timer = 0;
    std::uint8_t prescaler = 0;    // 5-bit
    std::uint8_t timer_mode = 0;   // bit0 timer, bit1 counter
    std::uint8_t timer_flag = 0;
    std::uint8_t t1_history = 0;

    std::uint8_t p1 = 0;
    std::uint8_t p2 = 0;
    std::uint8_t bus_latch = 0;

    bool f0 = false;
    bool f1 = false;
    bool t0 = false;
    bool t1 = false;

    bool irq_enabled = false;
    bool irq_pending = false;      // request latch captured from /INT
    bool irq_in_service = false;
    bool timer_irq_enabled = false;
    bool timer_irq_pending = false;

    std::uint64_t cycles = 0;      // absolute machine cycles executed
    std::uint16_t last_pc = 0;
    std::uint8_t illegal_opcode = 0;
    std::uint64_t illegal_count = 0;
};

struct StepResult {
    std::uint32_t cycles = 0;
    bool irq_taken = false;
};

} // namespace o2em::ng
