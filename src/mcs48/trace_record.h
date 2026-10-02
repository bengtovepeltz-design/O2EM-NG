#pragma once
// ============================================================================
// src/mcs48/trace_record.h
//
// MCS48-NG Phase 2A: deterministic CPU/bus trace record format.
//
// Design rules (Phase 1 report, section 10 / Phase 2 task):
//   - Tracing is DISABLED by default and must never mutate emulation state.
//     Recording is pure observation: callers copy values in, nothing calls
//     back into the emulator.
//   - Bounded: a fixed-capacity ring buffer keeps the newest records and
//     silently overwrites the oldest. No console spam, no unbounded memory.
//   - Deterministic: timestamps are ABSOLUTE EMULATED machine cycles only.
//     No wall-clock time, no thread scheduling dependence, no RNG.
//     Suitable for "stop at first divergence" differential work (VP61).
//   - Low intrusion: when disabled, recording costs one branch.
//
// Not wired into the production CPU yet: the legacy cpu.cpp remains the
// default and unchanged. Consumers (future Mcs48Ng core and the conformance
// harness) own when to enable the ring.
// ============================================================================

#include <cstdint>
#include <cstddef>

namespace mcs48 {

// Event kinds. Architectural snapshot fields (pc/a/psw/...) are meaningful
// for Instruction events; bus events carry address/value in payload fields.
enum class TraceEventKind : std::uint8_t {
    Instruction = 0,  // one executed instruction (pre-instruction state)
    ProgRead,         // program memory read where useful (payload = address, value)
    MovxRead,         // external data read  (payload = address, value)
    MovxWrite,        // external data write (payload = address, value)
    BusWrite,         // BUS latch/pins change (value = new BUS byte)
    P1Write,          // port 1 latch/pins change
    P2Write,          // port 2 latch/pins change
    IrqTransition,    // IRQ line state change (value = packed line/pending/in-service)
    VdcRead,          // VDC (8244/8245) register read
    VdcWrite,         // VDC (8244/8245) register write
};

// One trace record. Compact POD; all fields filled by the observer, never
// derived lazily, so a recorded stream can be diffed byte-for-byte.
struct TraceEvent {
    std::uint64_t seq = 0;            // monotonic sequence number (per ring)
    std::uint64_t machineCycles = 0;  // absolute emulated timestamp

    std::uint32_t pc = 0;             // pre-instruction PC (11-bit + bank bit)
    std::uint16_t payloadAddress = 0; // event-specific address (BUS/port/MOVX/VDC)
    std::uint8_t  payloadValue = 0;   // event-specific data value

    TraceEventKind kind = TraceEventKind::Instruction;

    std::uint8_t  opcode = 0;         // pre-instruction opcode (Instruction events)
    std::uint8_t  a = 0;              // accumulator
    std::uint8_t  psw = 0;            // PSW as assembled by the core
    std::uint8_t  timerValue = 0;     // 8-bit timer/counter register
    std::uint8_t  prescaler = 0;      // prescaler count (low bits suffice for fixtures)
    std::uint8_t  timerMode = 0;      // bit0 = timer running, bit1 = counter running
    std::uint8_t  irqFlags = 0;       // bit0 TF, bit1 xirq_en, bit2 tirq_en,
                                      // bit3 xirq_pend, bit4 tirq_pend,
                                      // bit5 irq in-service, bit6 F0, bit7 F1
    std::uint8_t  regBank = 0;        // register bank pointer (0 or 24)
    std::uint8_t  p1Latch = 0;        // port 1 output latch
    std::uint8_t  p2Latch = 0;        // port 2 output latch
    std::uint8_t  t0 = 0;             // T0 input pin
    std::uint8_t  t1 = 0;             // T1 input pin
    std::uint8_t  registers[8]{};     // Phase 2B: current register bank snapshot
};

// Bounded ring of trace events. Fixed capacity, no allocation, no locks:
// intended for the single-threaded deterministic execution path. Consumers
// iterate snapshots; the ring itself never touches emulation state.
class TraceRing {
public:
    static constexpr std::size_t kDefaultCapacity = 4096;

    explicit TraceRing(std::size_t capacity = kDefaultCapacity)
        : capacity_(capacity ? capacity : 1), storage_(new TraceEvent[capacity ? capacity : 1]) {}
    ~TraceRing() { delete[] storage_; }

    TraceRing(const TraceRing&) = delete;
    TraceRing& operator=(const TraceRing&) = delete;

    // Pure observation: copies a caller-built record. One branch when disabled.
    void Observe(const TraceEvent& event) {
        if (!enabled) return;
        TraceEvent& slot = storage_[writeIndex_];
        slot = event;
        slot.seq = ++sequence_;
        writeIndex_ = (writeIndex_ + 1) % capacity_;
        if (count_ < capacity_) ++count_;
    }

    // Snapshot iteration (oldest first). Does not consume.
    std::size_t Count() const { return count_; }
    const TraceEvent& At(std::size_t index) const {
        const std::size_t start = (writeIndex_ + capacity_ - count_) % capacity_;
        return storage_[(start + index) % capacity_];
    }
    void Clear() { writeIndex_ = 0; count_ = 0; sequence_ = 0; }

    // Global master switch, disabled by default. Kept separate from any ring
    // instance so hot paths can check one flag before building a record.
    static inline bool enabled = false;

private:
    const std::size_t capacity_;
    TraceEvent* storage_;
    std::size_t writeIndex_ = 0;
    std::size_t count_ = 0;
    std::uint64_t sequence_ = 0;
};

} // namespace mcs48
