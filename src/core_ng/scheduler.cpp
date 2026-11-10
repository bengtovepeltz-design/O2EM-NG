#include "scheduler.h"

#include <algorithm>

namespace o2em::ng {

void Scheduler::InsertSorted(const SchedulerEvent& event) {
    auto it = std::upper_bound(pending_.begin(), pending_.end(), event,
                               [](const SchedulerEvent& a, const SchedulerEvent& b) {
                                   return EventLess(a, b);
                               });
    pending_.insert(it, event);
}

void Scheduler::Dispatch(const SchedulerEvent& event) {
    const std::size_t index = static_cast<std::size_t>(event.handler);
    if (index < handlers_.size() && handlers_[index])
        handlers_[index](event);
}

std::size_t Scheduler::RunDue() {
    std::size_t executed = 0;
    while (!pending_.empty() && pending_.front().tick <= now_) {
        const SchedulerEvent event = pending_.front();
        pending_.erase(pending_.begin());
        Dispatch(event);
        ++executed;
    }
    return executed;
}

std::size_t Scheduler::AdvanceTo(Tick until) {
    std::size_t executed = 0;
    while (!pending_.empty() && pending_.front().tick <= until) {
        const SchedulerEvent event = pending_.front();
        pending_.erase(pending_.begin());
        Dispatch(event);
        ++executed;
    }
    if (until > now_) now_ = until;
    return executed;
}

bool Scheduler::SaveState(SnapshotWriter& w) const {
    w.u32(0x53434831u); // "SCH1"
    w.u64(static_cast<std::uint64_t>(now_));
    w.u64(sequence_);
    w.u32(static_cast<std::uint32_t>(pending_.size()));
    for (const auto& e : pending_) {
        w.u64(static_cast<std::uint64_t>(e.tick));
        w.u8(static_cast<std::uint8_t>(e.priority));
        w.u64(e.sequence);
        w.u8(static_cast<std::uint8_t>(e.kind));
        w.u32(static_cast<std::uint32_t>(e.handler));
        w.u32(e.payload);
    }
    return true;
}

bool Scheduler::LoadState(SnapshotReader& r) {
    if (r.u32() != 0x53434831u) return false;
    now_ = static_cast<Tick>(r.u64());
    sequence_ = r.u64();
    const std::uint32_t count = r.u32();
    if (!r.ok || count > 65536u) return false;
    pending_.clear();
    pending_.reserve(count);
    for (std::uint32_t i = 0; i < count; ++i) {
        SchedulerEvent e;
        e.tick = static_cast<Tick>(r.u64());
        e.priority = static_cast<EventPriority>(r.u8());
        e.sequence = r.u64();
        e.kind = static_cast<EventKind>(r.u8());
        e.handler = static_cast<HandlerId>(r.u32());
        e.payload = r.u32();
        pending_.push_back(e);
    }
    if (!r.ok) return false;
    // The queue is required to be in total order. Verify rather than sort so
    // a corrupt/incompatible snapshot is rejected deterministically.
    for (std::size_t i = 1; i < pending_.size(); ++i) {
        if (EventLess(pending_[i], pending_[i - 1])) return false;
    }
    return true;
}

} // namespace o2em::ng
