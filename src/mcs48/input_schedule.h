#pragma once
// ============================================================================
// src/mcs48/input_schedule.h
//
// MCS48-NG Phase 2A: infrastructure to identify deterministic input events.
//
// Goal (Phase 1 section 6): reproduce fixture inputs such as the VP61
// "reset / Select Game / key 0 press / key 0 release" sequence at EXACT
// emulated machine-cycle timestamps instead of human wall-clock timing.
//
// Scope limits:
//   - Pure data structure. Nothing in the emulator or frontend consumes it
//     yet; wiring a frontend hook is documented in the Phase 2A note as
//     future work and was deliberately NOT implemented here to avoid
//     invasive frontend changes.
//   - No game-specific behavior, no CRC checks, no VP61 knowledge.
//   - Key codes are abstract (frontend-dependent); interpretation belongs
//     to the future consumer.
//
// Determinism: events are keyed by ABSOLUTE emulated machine cycles, never
// wall-clock time. A schedule replayed against the same ROM/BIOS/settings
// must produce bit-identical machine input timing.
// ============================================================================

#include <cstdint>
#include <vector>

namespace mcs48 {

enum class InputAction : std::uint8_t {
    KeyDown = 0,
    KeyUp = 1,
};

struct ScheduledInput {
    std::uint64_t atMachineCycle = 0; // absolute emulated timestamp
    std::uint16_t key = 0;            // abstract key/controller code
    InputAction action = InputAction::KeyDown;
};

class InputSchedule {
public:
    // Bounded by design; differential fixtures need handfuls of events, not
    // thousands. Exceeding the bound is a programming error and is refused.
    static constexpr std::size_t kMaxEvents = 256;

    void Reset() {
        events_.clear();
        next_ = 0;
    }

    // Inserts keeping ascending timestamp order. Returns false on overflow,
    // on a timestamp earlier than an already-consumed event (non-monotonic
    // mutation of a running schedule), or on an exact duplicate slot.
    bool Add(std::uint64_t atMachineCycle, std::uint16_t key, InputAction action) {
        if (events_.size() >= kMaxEvents) return false;
        if (next_ > 0 && atMachineCycle < events_[next_ - 1].atMachineCycle) return false;
        ScheduledInput event{atMachineCycle, key, action};
        std::size_t pos = events_.size();
        while (pos > next_ && events_[pos - 1].atMachineCycle > atMachineCycle) {
            if (events_[pos - 1].atMachineCycle == atMachineCycle &&
                events_[pos - 1].key == key) return false;
            --pos;
        }
        events_.insert(events_.begin() + static_cast<std::ptrdiff_t>(pos), event);
        return true;
    }

    bool Empty() const { return next_ >= events_.size(); }

    // Next event regardless of current time (nullptr when exhausted).
    const ScheduledInput* PeekNext() const {
        return next_ < events_.size() ? &events_[next_] : nullptr;
    }

    // Pops the next event when its timestamp has arrived (<= machineCycles).
    bool PopDue(std::uint64_t machineCycles, ScheduledInput& out) {
        if (next_ >= events_.size()) return false;
        if (events_[next_].atMachineCycle > machineCycles) return false;
        out = events_[next_++];
        return true;
    }

private:
    std::vector<ScheduledInput> events_;
    std::size_t next_ = 0;
};

} // namespace mcs48
