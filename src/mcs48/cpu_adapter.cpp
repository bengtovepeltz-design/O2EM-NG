// ============================================================================
// src/mcs48/cpu_adapter.cpp
//
// MCS48-NG Phase 2A: adapter implementations.
//
// LegacyCpuAdapter forwards to the existing global functions in cpu.cpp.
// It introduces no state, no locking, no timing change and no extra
// indirection beyond a virtual call - production behavior is untouched and
// the legacy CPU remains the default (nothing calls this from the emulator
// yet; the adapter exists so a later phase can swap implementations behind
// one contract without touching cpu.cpp).
// ============================================================================

#include "cpu_adapter.h"

#include "cpu.h"

namespace mcs48 {

void LegacyCpuAdapter::Init() {
    ::init_cpu();
}

void LegacyCpuAdapter::ExtIrq() {
    ::ext_IRQ();
}

void LegacyCpuAdapter::TimIrq() {
    ::tim_IRQ();
}

unsigned LegacyCpuAdapter::ReadPc() const {
    return static_cast<unsigned>(::pc);
}

void Mcs48NgAdapter::Init() {
    state_ = State{}; // placeholder: real reset contract arrives with the core
}

void Mcs48NgAdapter::ExtIrq() {
    // Placeholder: no interrupt machinery implemented yet.
}

void Mcs48NgAdapter::TimIrq() {
    // Placeholder: no timer/IRQ machinery implemented yet.
}

unsigned Mcs48NgAdapter::ReadPc() const {
    return state_.pc;
}

} // namespace mcs48
