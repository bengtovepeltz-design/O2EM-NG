#pragma once
// ============================================================================
// src/mcs48/cpu_adapter.h
//
// MCS48-NG Phase 2A: minimal CPU interface contract, per Phase 1 section 10
// and the Phase 2 task definition.
//
// IMPORTANT SCOPE LIMITS
//   - The legacy cpu.cpp remains the ONLY production CPU. Nothing in the
//     emulator dispatches through this interface; there is no user-facing
//     selector. "Legacy default" is preserved by absence of any switch.
//   - LegacyCpuAdapter is a pure forwarder onto the existing global-state
//     functions. It adds no state, no timing change, no indirection with
//     behavior. If forwarding ever required changing legacy semantics, the
//     task instruction was to STOP and report - it did not.
//   - Mcs48NgAdapter is a skeleton only: state placeholder and placeholders
//     that report "not implemented". It contains NO opcode implementation
//     and must never be selected for production emulation.
//
// Why there is no Step() on the shared contract: the legacy core has no
// owned single-instruction entry point. Its cpu_exec() is frame-bounded and
// its existing single-step behavior is tied to app_data.debug, which is
// production configuration, not an adapter feature. The conformance harness
// uses that existing path directly; a future MCS48-NG core will own a real
// Step() on its own adapter.
// ============================================================================

#include <cstdint>

#include "types.h"

namespace mcs48 {

class ICpu {
public:
    virtual ~ICpu() = default;

    // Cold CPU reset (CPU-visible state only; machine reset is separate).
    virtual void Init() = 0;
    // External interrupt pulse handling (synthetic /INT in the legacy core).
    virtual void ExtIrq() = 0;
    // Timer/counter overflow service attempt.
    virtual void TimIrq() = 0;
    // Read-only observation of the current program counter.
    virtual unsigned ReadPc() const = 0;
};

// Wraps the existing legacy CPU exactly as-is. Pure forwarding; semantics,
// timing and global state usage are unchanged.
class LegacyCpuAdapter final : public ICpu {
public:
    void Init() override;
    void ExtIrq() override;
    void TimIrq() override;
    unsigned ReadPc() const override;
};

// Skeleton for the future MCS48-NG core. Placeholder state and entry points
// that do nothing except report that they are not implemented. No opcode
// engine lives here yet.
class Mcs48NgAdapter final : public ICpu {
public:
    enum class StepStatus { NotImplemented, Ok };

    // Placeholder owned CPU state. Phase 1 section 10 defines what belongs
    // here eventually; deliberately left almost empty for now.
    struct State {
        unsigned pc = 0;
        std::uint8_t a = 0;
        std::uint8_t psw = 0;
        std::uint8_t timer = 0;
        std::uint8_t prescaler = 0;
    };

    void Init() override;
    void ExtIrq() override;
    void TimIrq() override;
    unsigned ReadPc() const override;

    // Development-only execute placeholder. Not part of ICpu: the legacy
    // core cannot provide it without changing production configuration.
    StepStatus Step(std::uint64_t /*machineCycles*/) { return StepStatus::NotImplemented; }

    const State& PeekState() const { return state_; }

private:
    State state_{};
};

} // namespace mcs48
