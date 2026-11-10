#pragma once
// NG-0 diagnostics bridge. Reuses the existing bounded trace concepts.

#include <cstdint>

#include "../../mcs48/trace_record.h"

namespace o2em::ng {

struct TraceHooks {
    // The ring is disabled by default and observation must not mutate state.
    mcs48::TraceRing* ring = nullptr;

    void Observe(const mcs48::TraceEvent& event) {
        if (ring && mcs48::TraceRing::enabled)
            ring->Observe(event);
    }
};

} // namespace o2em::ng
