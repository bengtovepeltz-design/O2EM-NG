#pragma once
// O2EM-NG compact, deterministic bus tracing.
//
// Disabled by default. Every record is a small POD value; filters are applied
// at record time so a runaway configuration can never emit an unbounded log.

#include <cstddef>
#include <cstdint>
#include <vector>

#include "../state.h"
#include "../timebase.h"

namespace o2em::ng {

enum class BusOp : std::uint8_t {
    None = 0,
    ProgramFetch,
    MovxRead,
    MovxWrite,
    PortRead,
    PortWrite,
    BusRead,
    BusWrite,
    IrqTransition,
    T0Transition,
    T1Transition,
    MapperWrite
};

enum class BusDevice : std::uint8_t {
    None = 0,
    Bios,
    Cartridge,
    ExtRam,
    Vdc,
    Vpp,
    Mapper,
    OpenBus,
    Port,
    Bus,
    Cpu,
    Special
};

const char* BusOpName(BusOp op);
const char* BusDeviceName(BusDevice device);

struct BusRecord {
    Tick tick = 0;
    std::uint64_t cpu_cycle = 0;
    std::uint16_t pc = 0;
    BusOp op = BusOp::None;
    std::uint16_t address = 0;
    std::uint8_t data = 0;
    std::uint8_t p1 = 0;
    std::uint8_t p2 = 0;
    std::uint8_t bank = 0;
    BusDevice device = BusDevice::None;
};

struct BusTraceFilter {
    Tick from_tick = 0;
    Tick to_tick = INT64_MAX;
    std::uint16_t pc_min = 0x0000;
    std::uint16_t pc_max = 0x0FFF;
    std::uint16_t op_mask = 0xFFFF; // bit (1 << BusOp)
    std::size_t max_records = 4096;
    bool include_program_fetch = true;
};

class BusTrace {
public:
    void Reset();
    void SetEnabled(bool enabled) { enabled_ = enabled; }
    bool Enabled() const { return enabled_; }
    void SetFilter(const BusTraceFilter& filter) { filter_ = filter; }
    const BusTraceFilter& Filter() const { return filter_; }

    void Record(const BusRecord& record);
    const std::vector<BusRecord>& Records() const { return records_; }
    std::size_t Size() const { return records_.size(); }
    bool Truncated() const { return truncated_; }

private:
    bool Accept(const BusRecord& record) const;

    bool enabled_ = false;
    bool truncated_ = false;
    BusTraceFilter filter_{};
    std::vector<BusRecord> records_;
};

} // namespace o2em::ng
