#include "bus_trace.h"

namespace o2em::ng {

const char* BusOpName(BusOp op) {
    switch (op) {
    case BusOp::None: return "none";
    case BusOp::ProgramFetch: return "fetch";
    case BusOp::MovxRead: return "movx-r";
    case BusOp::MovxWrite: return "movx-w";
    case BusOp::PortRead: return "port-r";
    case BusOp::PortWrite: return "port-w";
    case BusOp::BusRead: return "bus-r";
    case BusOp::BusWrite: return "bus-w";
    case BusOp::IrqTransition: return "irq";
    case BusOp::T0Transition: return "t0";
    case BusOp::T1Transition: return "t1";
    case BusOp::MapperWrite: return "mapper-w";
    }
    return "?";
}

const char* BusDeviceName(BusDevice device) {
    switch (device) {
    case BusDevice::None: return "none";
    case BusDevice::Bios: return "bios";
    case BusDevice::Cartridge: return "cart";
    case BusDevice::ExtRam: return "extram";
    case BusDevice::Vdc: return "vdc";
    case BusDevice::Vpp: return "vpp";
    case BusDevice::Mapper: return "mapper";
    case BusDevice::OpenBus: return "open";
    case BusDevice::Port: return "port";
    case BusDevice::Bus: return "bus";
    case BusDevice::Cpu: return "cpu";
    case BusDevice::Special: return "special";
    }
    return "?";
}

void BusTrace::Reset() {
    records_.clear();
    truncated_ = false;
}

bool BusTrace::Accept(const BusRecord& record) const {
    if (!enabled_) return false;
    if (record.op == BusOp::ProgramFetch && !filter_.include_program_fetch)
        return false;
    if (record.tick < filter_.from_tick || record.tick > filter_.to_tick)
        return false;
    if (record.pc < filter_.pc_min || record.pc > filter_.pc_max)
        return false;
    if ((filter_.op_mask & (1u << static_cast<unsigned>(record.op))) == 0)
        return false;
    return true;
}

void BusTrace::Record(const BusRecord& record) {
    if (!Accept(record)) return;
    if (records_.size() >= filter_.max_records) {
        truncated_ = true;
        return;
    }
    records_.push_back(record);
}

} // namespace o2em::ng
