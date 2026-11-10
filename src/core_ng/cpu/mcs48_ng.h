#pragma once
// NG-0 MCS48-NG production CPU contract.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "mcs48_bus.h"
#include "mcs48_state.h"

namespace o2em::ng {

class Mcs48Cpu {
public:
    explicit Mcs48Cpu(Mcs48Bus& bus) : bus_(bus) {}

    void PowerOn();
    void Reset();
    StepResult Step();                 // exactly one instruction

    void SetIrqLevel(bool asserted);   // /INT level, latched into irq_pending
    void AcknowledgeIrq();            // i8244 A1 read / line clear
    void SetT0(bool level);
    void SetT1(bool level);

    const Mcs48State& State() const { return state_; }
    Mcs48State& MutableState() { return state_; }

    void SaveState(std::vector<std::uint8_t>& out) const;
    bool LoadState(const std::uint8_t* data, std::size_t size);

private:
    Mcs48Bus& bus_;
    Mcs48State state_{};

    void Burn(std::uint32_t count);
    void TimerOverflow();
    bool CheckIrqs();
    std::uint8_t Fetch();
    void Push();
    void PopPc();
    void PopPcPsw();
    void Call(std::uint16_t target);
    void Jump(std::uint16_t target);
    void Branch(bool condition);
    void JumpPage(int page);
    void CallPage(int page);
    void Add(std::uint8_t value, bool carryIn);
    void Da();
    std::uint32_t Execute(std::uint8_t opcode);
};

} // namespace o2em::ng
