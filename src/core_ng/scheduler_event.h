#pragma once
// O2EM-NG deterministic scheduler event description.

#include <cstdint>

#include "timebase.h"

namespace o2em::ng {

enum class EventKind : std::uint8_t {
    CpuInstructionBoundary = 0,
    CpuBusAccess,
    IrqTransition,
    TimerTick,
    HblankStart,
    HblankEnd,
    VblankStart,
    VblankEnd,
    FrameEnd,
    AudioTick,
    VppTick,
    Presentation,
    InputTransition,
    TestTransition
};

// Lower value runs first at the same Tick. Sequence breaks remaining ties.
enum class EventPriority : std::uint8_t {
    Cpu = 0,
    Irq = 1,
    Video = 2,
    Timer = 3,
    Input = 4,
    Audio = 5,
    Presentation = 6
};

// Handler IDs are stable small integers owned by the Machine. Keeping them
// integer (rather than std::function) makes the pending queue trivially
// snapshot-able.
enum class HandlerId : std::uint32_t {
    None = 0,
    VblankIrq = 1,
    FrameEnd = 2,
    InputTransition = 3,
    TestTransition = 4,
    Heartbeat = 5
};

struct SchedulerEvent {
    Tick tick = 0;
    EventPriority priority = EventPriority::Video;
    std::uint64_t sequence = 0;
    EventKind kind = EventKind::Presentation;
    HandlerId handler = HandlerId::None;
    std::uint32_t payload = 0;
};

inline bool EventLess(const SchedulerEvent& a, const SchedulerEvent& b) {
    if (a.tick != b.tick) return a.tick < b.tick;
    if (a.priority != b.priority)
        return static_cast<int>(a.priority) < static_cast<int>(b.priority);
    return a.sequence < b.sequence;
}

} // namespace o2em::ng
