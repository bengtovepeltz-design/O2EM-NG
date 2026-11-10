#pragma once

#include "src/core_ng/special/special_hardware.h"

// C7010 chess module as NG special hardware.
//
// The device implementation is the existing C7010 NSC800 module (c7010.cpp).
// This adapter exposes only the machine wiring points defined by
// o2em::ng::SpecialHardware. No frontend, CPU-core or title-specific code is
// involved.
class C7010SpecialHardware final : public o2em::ng::SpecialHardware {
public:
    void PowerOn() override;
    void Reset() override;
    void WriteP1(std::uint8_t value) override;
    bool ExternalRead(std::uint8_t address, std::uint8_t& value) override;
    bool ExternalWrite(std::uint8_t address, std::uint8_t value) override;
    void RunCpuCycles(unsigned cycles, unsigned frame_cycles) override;
    void FrameTick() override;
};
