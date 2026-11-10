#pragma once
// NG-0 CPU-facing machine bus contract. The CPU knows nothing else.

#include <cstdint>

namespace o2em::ng {

class Mcs48Bus {
public:
    virtual ~Mcs48Bus() = default;

    virtual std::uint8_t ProgramRead(std::uint16_t address) = 0;
    virtual std::uint8_t DataRead(std::uint8_t address) = 0;
    virtual void DataWrite(std::uint8_t address, std::uint8_t value) = 0;

    virtual std::uint8_t BusRead() = 0;
    virtual void BusWrite(std::uint8_t value) = 0;

    virtual std::uint8_t PortRead(int port) = 0;   // 1 or 2
    virtual void PortWrite(int port, std::uint8_t value) = 0;

    virtual std::uint8_t ExpanderRead(int port) = 0;   // 8243 P4..P7
    virtual void ExpanderWrite(int port, std::uint8_t value) = 0;
    virtual void ProgWrite(bool level) = 0;            // 8243 PROG
    virtual void T0Clock(bool enabled) = 0;            // ENT0 CLK
};

} // namespace o2em::ng
