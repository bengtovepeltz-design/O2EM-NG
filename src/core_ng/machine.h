#pragma once
// O2EM-NG public Machine API.
//
// The frontend talks only to this boundary. The machine owns the CPU, bus,
// scheduler, timebase, BIOS, cartridge, mapper, external RAM, input state,
// stub VDC and snapshot state. No SDL, frontend, database or title knowledge.

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "cpu/mcs48_state.h"
#include "machine_config.h"
#include "memory/bus.h"
#include "scheduler.h"
#include "special/special_hardware.h"
#include "state.h"
#include "video/framebuffer.h"

namespace o2em::ng {

class Vpp;
class AudioDevice;

struct InputState {
    // Legacy/frontend-facing convenience form. p1_buttons/p2_buttons use the
    // ControllerBits layout; keyboard rows are active-high.
    std::uint8_t p1_buttons = 0;
    std::uint8_t p2_buttons = 0;
    std::uint8_t keyboard[8] = {};
    bool reset = false;
};

class MachineHost {
public:
    virtual ~MachineHost() = default;
    virtual void PresentFrame(const Framebuffer&) = 0;
    virtual void PushAudio(const std::int16_t* samples, std::size_t count) = 0;
    virtual void OnFrameComplete(std::uint64_t frame_number) = 0;
};

class Machine {
public:
    static std::unique_ptr<Machine> Create(const MachineConfig&, MachineHost&);
    virtual ~Machine() = default;

    virtual bool Load() = 0;
    virtual bool LoadBiosBytes(std::vector<std::uint8_t> bytes,
                               std::string name = {}) = 0;
    virtual bool LoadCartridgeBytes(std::vector<std::uint8_t> bytes,
                                    std::string name = {},
                                    std::string path = {}) = 0;

    virtual void PowerOn() = 0;
    virtual void Reset() = 0;

    // Attach cartridge-required external hardware (for example the C7010
    // chess module). Must be called after Load()/LoadCartridgeBytes() and
    // before PowerOn(). Passing nullptr detaches. The machine owns the device.
    virtual void AttachSpecialHardware(
        std::unique_ptr<SpecialHardware> hardware) = 0;

    virtual void RunFrame() = 0;
    virtual void RunCycles(std::uint64_t cpu_cycles) = 0;
    virtual void RunInstructions(std::uint64_t instructions) = 0;
    virtual void RunUntil(Tick tick) = 0;

    virtual void SetInput(const InputState&) = 0;
    virtual void SetMachineInput(const MachineInput&) = 0;
    virtual bool ScheduleInput(const InputTransition&) = 0;
    virtual bool SchedulePinTransition(Tick tick, bool t1_pin, bool level) = 0;
    virtual void SetPinOverrides(bool t0_enabled, bool t0_level,
                                 bool t1_enabled, bool t1_level) = 0;

    virtual const Framebuffer& GetFramebuffer() const = 0;
    virtual MachineInfo GetInfo() const = 0;

    virtual bool SaveState(std::vector<std::uint8_t>&) const = 0;
    virtual bool LoadState(const std::uint8_t*, std::size_t) = 0;

    // Deterministic inspection for tests and the MAME differential.
    virtual Tick Now() const = 0;
    virtual const HardwareProfile& Profile() const = 0;
    virtual const Mcs48State& CpuState() const = 0;
    virtual const Scheduler& GetScheduler() const = 0;
    virtual const MachineBus& Bus() const = 0;
    virtual const Cartridge& Cart() const = 0;
    virtual const Mapper* MapperState() const = 0;
    virtual const Vdc& Vdc() const = 0;
    virtual const Vpp& Vpp() const = 0;
    virtual const AudioDevice& Audio() const = 0;
    virtual BusTrace& MutableTrace() = 0;
    virtual const BusTrace& Trace() const = 0;
};

} // namespace o2em::ng
