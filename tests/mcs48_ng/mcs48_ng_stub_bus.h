#pragma once
// Deterministic stub bus for the standalone MCS48-NG conformance target.

#include <cstdint>
#include <cstring>

#include "cpu/mcs48_bus.h"

namespace mcs48_ng_test {

class TestBus final : public o2em::ng::Mcs48Bus {
public:
    std::uint8_t program[4096]{};
    std::uint8_t data[256]{};
    std::uint8_t port_in[3]{0, 0xFF, 0xFF};
    std::uint8_t port_out[3]{};
    std::uint8_t expander[8]{};
    std::uint8_t bus_value = 0xFF;
    bool t0_clock = false;
    int bus_writes = 0;
    int data_writes = 0;
    int expander_writes = 0;
    int prog_writes = 0;

    void Reset() {
        std::memset(program, 0, sizeof(program));
        std::memset(data, 0, sizeof(data));
        std::memset(port_out, 0, sizeof(port_out));
        std::memset(expander, 0, sizeof(expander));
        port_in[1] = port_in[2] = 0xFF;
        bus_value = 0xFF;
        t0_clock = false;
        bus_writes = data_writes = expander_writes = prog_writes = 0;
    }

    std::uint8_t ProgramRead(std::uint16_t address) override {
        return program[address & 0x0FFF];
    }
    std::uint8_t DataRead(std::uint8_t address) override { return data[address]; }
    void DataWrite(std::uint8_t address, std::uint8_t value) override {
        data[address] = value;
        ++data_writes;
    }
    std::uint8_t BusRead() override { return bus_value; }
    void BusWrite(std::uint8_t value) override { bus_value = value; ++bus_writes; }
    std::uint8_t PortRead(int port) override {
        return (port >= 1 && port <= 2) ? port_in[port] : 0xFF;
    }
    void PortWrite(int port, std::uint8_t value) override {
        if (port >= 1 && port <= 2) port_out[port] = value;
    }
    void ProgWrite(bool) override { ++prog_writes; }
    std::uint8_t ExpanderRead(int port) override {
        return (port >= 4 && port <= 7) ? expander[port] : 0;
    }
    void ExpanderWrite(int port, std::uint8_t value) override {
        if (port >= 4 && port <= 7) {
            expander[port] = value;
            ++expander_writes;
        }
    }
    void T0Clock(bool enabled) override { t0_clock = enabled; }
};

} // namespace mcs48_ng_test
