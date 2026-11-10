#pragma once
// O2EM-NG deterministic machine scheduler.
//
// Ordering is explicit and total: (tick, priority, sequence). The pending
// queue is kept as a small vector so that the complete scheduling state can
// be snapshotted and restored byte-for-byte. No std::function is stored in
// the queue; events carry a stable HandlerId resolved by the Machine.

#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

#include "scheduler_event.h"
#include "state.h"
#include "timebase.h"

namespace o2em::ng {

class Scheduler {
public:
    using Handler = std::function<void(const SchedulerEvent&)>;

    Scheduler() = default;
    explicit Scheduler(HardwareProfile profile) : profile_(profile) {}

    Tick Now() const { return now_; }
    void SetNow(Tick tick) { now_ = tick; }
    const HardwareProfile& Profile() const { return profile_; }
    void SetProfile(const HardwareProfile& p) { profile_ = p; }

    void Register(HandlerId id, Handler handler) {
        const std::size_t index = static_cast<std::size_t>(id);
        if (handlers_.size() <= index) handlers_.resize(index + 1);
        handlers_[index] = std::move(handler);
    }

    void Schedule(Tick when, EventPriority priority, EventKind kind,
                  HandlerId handler, std::uint32_t payload = 0) {
        SchedulerEvent event;
        event.tick = when;
        event.priority = priority;
        event.sequence = ++sequence_;
        event.kind = kind;
        event.handler = handler;
        event.payload = payload;
        InsertSorted(event);
    }

    // Executes every pending event with tick <= until, in total order, then
    // sets Now() = until. Returns the number of events executed.
    std::size_t AdvanceTo(Tick until);

    // Executes only due events (tick <= Now()) without moving Now.
    std::size_t RunDue();

    // Cancels every pending event. Handlers stay registered.
    void Clear() { pending_.clear(); sequence_ = 0; }

    std::size_t PendingCount() const { return pending_.size(); }
    const SchedulerEvent* PeekNext() const {
        return pending_.empty() ? nullptr : &pending_.front();
    }
    const std::vector<SchedulerEvent>& Pending() const { return pending_; }

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

private:
    void InsertSorted(const SchedulerEvent& event);
    void Dispatch(const SchedulerEvent& event);

    HardwareProfile profile_{};
    Tick now_ = 0;
    std::uint64_t sequence_ = 0;
    std::vector<SchedulerEvent> pending_; // always sorted by EventLess
    std::vector<Handler> handlers_;
};

} // namespace o2em::ng
