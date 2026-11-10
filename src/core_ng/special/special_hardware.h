#pragma once
// O2EM-NG special-hardware boundary.
//
// Some cartridges contain or require external hardware that is not part of the
// base G7000/G7400 machine: the C7010 chess module (NSC800 + latches) is the
// first user. The NG machine exposes only the wiring points that such a device
// needs. The device implementation itself lives outside src/core_ng, exactly
// like the cartridge mapper does not know about SDL or the frontend.
//
// All methods are optional no-ops for machines without special hardware.

#include <cstdint>

namespace o2em::ng {

class SpecialHardware {
public:
    virtual ~SpecialHardware() = default;

    // Power-on and frontend reset both drive the device's reset line.
    virtual void PowerOn() = 0;
    virtual void Reset() = 0;

    // 8048 P1 output. The device sees the raw port value.
    virtual void WriteP1(std::uint8_t value) = 0;

    // MOVX read/write window. Return true when the device drives/claims the
    // access; the machine then suppresses the normal cartridge/VDC response.
    virtual bool ExternalRead(std::uint8_t address, std::uint8_t& value) = 0;
    virtual bool ExternalWrite(std::uint8_t address, std::uint8_t value) = 0;

    // Interleaved coprocessor execution. cycles is the 8048 machine-cycle
    // count of the instruction just executed; frame_cycles is the 8048
    // machine-cycle budget of one video frame.
    virtual void RunCpuCycles(unsigned cycles, unsigned frame_cycles) = 0;

    // Once per completed video frame (observation/refresh bookkeeping).
    virtual void FrameTick() = 0;
};

} // namespace o2em::ng
