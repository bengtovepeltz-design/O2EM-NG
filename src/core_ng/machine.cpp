#include "machine.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <utility>

#include "cpu/mcs48_ng.h"
#include "audio/audio_device.h"
#include "video/vpp.h"

namespace o2em::ng {

namespace {

constexpr std::uint32_t kSnapshotMagic = 0x4E474D32u; // "NGM2"
constexpr std::uint32_t kSnapshotVersion = 4;

bool ReadFileBytes(const std::string& path, std::vector<std::uint8_t>& out) {
    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, 0, SEEK_END) != 0) {
        std::fclose(f);
        return false;
    }
    const long length = std::ftell(f);
    if (length <= 0) {
        std::fclose(f);
        return false;
    }
    std::rewind(f);
    out.resize(static_cast<std::size_t>(length));
    const std::size_t read = std::fread(out.data(), 1, out.size(), f);
    std::fclose(f);
    return read == out.size();
}

std::string BaseName(const std::string& path) {
    const std::size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

ControllerState ControllerFromBits(std::uint8_t bits) {
    ControllerState c;
    c.up = (bits & 0x01) != 0;
    c.right = (bits & 0x02) != 0;
    c.down = (bits & 0x04) != 0;
    c.left = (bits & 0x08) != 0;
    c.fire = (bits & 0x10) != 0;
    return c;
}

class NgMachine final : public Machine {
public:
    NgMachine(const MachineConfig& config, MachineHost& host)
        : config_(config), host_(&host) {
        profile_ = config.use_custom_profile
                       ? config.custom_profile
                       : (config.g7400 ? MakeG7400Profile()
                                       : MakeProfile(config.region));
        scheduler_.SetProfile(profile_);
        scheduler_.Register(HandlerId::VblankIrq, [this](const SchedulerEvent& e) {
            // The VBL phase is the same authoritative tick that drives the
            // raster. Advance the device to the exact event tick before
            // latching the status/IRQ so the pre-VBL pixels use the pre-IRQ
            // register state.
            vdc_.AdvanceTo(e.tick);
            vdc_.AssertVblankIrq();
            // Steady-state phase: the first event after reset may be at the
            // end of the partial first frame, all later frames assert at the
            // start of VBLANK (see HardwareProfile).
            const Tick frame = profile_.FrameTicks();
            const Tick next = ((e.tick / frame) + 1) * frame +
                              profile_.VblankIrqOffset();
            scheduler_.Schedule(next, EventPriority::Irq,
                                EventKind::VblankStart, HandlerId::VblankIrq);
        });
        scheduler_.Register(HandlerId::InputTransition, [this](const SchedulerEvent& e) {
            ApplyInputPayload(e.payload);
        });
        scheduler_.Register(HandlerId::TestTransition, [this](const SchedulerEvent& e) {
            if (e.payload & 0x80000000u)
                bus_.SetT1Override(true, (e.payload & 1u) != 0);
            else
                bus_.SetT0Override(true, (e.payload & 1u) != 0);
        });
        scheduler_.Register(HandlerId::FrameEnd, [](const SchedulerEvent&) {});
        scheduler_.Register(HandlerId::Heartbeat, [](const SchedulerEvent&) {});
    }

    bool Load() override {
        if (config_.bios_path.empty()) return false;
        std::vector<std::uint8_t> bios;
        if (!ReadFileBytes(config_.bios_path, bios)) return false;
        if (!LoadBiosBytes(std::move(bios), BaseName(config_.bios_path)))
            return false;

        if (!config_.cartridge_path.empty()) {
            std::vector<std::uint8_t> cart;
            if (!ReadFileBytes(config_.cartridge_path, cart)) return false;
            if (!LoadCartridgeBytes(std::move(cart),
                                    BaseName(config_.cartridge_path),
                                    config_.cartridge_path))
                return false;
        }
        return true;
    }

    bool LoadBiosBytes(std::vector<std::uint8_t> bytes,
                       std::string name) override {
        if (bytes.empty() || bytes.size() > 0x0400) return false;
        bios_ = std::move(bytes);
        bios_name_ = std::move(name);
        return true;
    }

    bool LoadCartridgeBytes(std::vector<std::uint8_t> bytes, std::string name,
                            std::string path) override {
        Cartridge candidate;
        if (!candidate.Load(std::move(bytes), std::move(name), std::move(path)))
            return false;
        if (candidate.info.kind == CartridgeKind::Unsupported) {
            cart_.Clear();
            mapper_.reset();
            return false;
        }
        cart_ = std::move(candidate);
        vpp_.SetBPin(cart_.info.b_pin);
        mapper_ = MakeMapper(cart_);
        if (!mapper_) {
            cart_.Clear();
            return false;
        }
        mapper_->Reset(cart_);
        if (powered_) Reset();
        return true;
    }

    void AttachSpecialHardware(
        std::unique_ptr<SpecialHardware> hardware) override {
        special_ = std::move(hardware);
        if (powered_) PowerOn();
    }

    void PowerOn() override {
        profile_ = config_.use_custom_profile
                       ? config_.custom_profile
                       : (config_.g7400 ? MakeG7400Profile()
                                        : MakeProfile(config_.region));
        scheduler_.SetProfile(profile_);
        vdc_.Configure(profile_);
        audio_.Configure(profile_, 48000);
        vdc_.AttachAudio(&audio_);
        if (config_.vpp_enabled) {
            vpp_.Configure(profile_);
            vpp_.SetBPin(cart_.info.b_pin);
            vdc_.AttachVpp(&vpp_);
        } else {
            vdc_.AttachVpp(nullptr);
        }

        bus_.Configure(profile_, &bios_, &cart_, mapper_.get(), &vdc_,
                       config_.vpp_enabled ? &vpp_ : nullptr, &input_, &trace_,
                       special_.get());
        bus_.PowerOn();
        if (special_) special_->PowerOn();
        cpu_.PowerOn();
        // MCS-48 RESET/ports: quasi-bidirectional input mode. Keep the CPU
        // latch and the external bus in agreement.
        cpu_.MutableState().p1 = 0xFF;
        cpu_.MutableState().p2 = 0xFF;
        if (mapper_) mapper_->WriteP1(0xFF);

        input_ = MachineInput{};
        trace_.Reset();
        now_ = 0;
        audio_.Reset(0);
        frame_count_ = 0;
        powered_ = true;

        scheduler_.Clear();
        scheduler_.SetNow(0);
        const Tick first_vblank = profile_.FirstVblankIrqOffset();
        scheduler_.Schedule(first_vblank, EventPriority::Irq,
                            EventKind::VblankStart, HandlerId::VblankIrq);

        bus_.SetAccessContext(now_, cpu_.State().pc);
        last_irq_ = bus_.IrqLine();
        last_t0_ = bus_.T0();
        last_t1_ = bus_.T1();
        SyncCpuInputs();
    }

    void Reset() override {
        if (!powered_) {
            PowerOn();
            return;
        }
        cpu_.Reset();
        cpu_.MutableState().p1 = 0xFF;
        cpu_.MutableState().p2 = 0xFF;

        if (mapper_) {
            mapper_->Reset(cart_);
            // MAME's i8048 reset drives P1=0xFF through p1_write, so the
            // cartridge sees the same reset bank.
            mapper_->WriteP1(0xFF);
        }
        bus_.Reset();
        if (special_) special_->Reset();
        vdc_.Reset();
        vdc_.SetTick(now_);
        audio_.Reset(now_);
        if (config_.vpp_enabled) {
            vpp_.Reset();
            vpp_.SetBPin(cart_.info.b_pin);
        }

        // RESET does not clear external RAM, input state or machine time.
        // It does close any open IRQ latch in the VDC.
        bus_.SetAccessContext(now_, cpu_.State().pc);
        SyncCpuInputs();
        last_irq_ = bus_.IrqLine();
        last_t0_ = bus_.T0();
        last_t1_ = bus_.T1();

        bool has_vblank = false;
        for (const auto& e : scheduler_.Pending()) {
            if (e.handler == HandlerId::VblankIrq) {
                has_vblank = true;
                break;
            }
        }
        if (!has_vblank) {
            const Tick frame = profile_.FrameTicks();
            const Tick phase = now_ % frame;
            Tick next = now_ - phase + profile_.VblankIrqOffset();
            if (next <= now_) next += frame;
            scheduler_.Schedule(next, EventPriority::Irq, EventKind::VblankStart,
                                HandlerId::VblankIrq);
        }
    }

    void RunFrame() override {
        const Tick frame = profile_.FrameTicks();
        const Tick target = ((now_ / frame) + 1) * frame;
        RunUntil(target);
        vdc_.AdvanceTo(now_);
        audio_.AdvanceTo(now_);
        if (special_) special_->FrameTick();
        ++frame_count_;
        if (host_) {
            host_->PresentFrame(vdc_.Framebuffer());
            audio_frame_.clear();
            audio_.Drain(audio_frame_);
            if (!audio_frame_.empty())
                host_->PushAudio(audio_frame_.data(), audio_frame_.size());
            host_->OnFrameComplete(frame_count_);
        }
    }

    void RunCycles(std::uint64_t cpu_cycles) override {
        if (cpu_cycles == 0) return;
        if (!powered_) PowerOn();
        const Tick target = now_ + static_cast<Tick>(cpu_cycles) * kTicksPerCpuCycle;
        RunUntil(target);
    }

    void RunInstructions(std::uint64_t instructions) override {
        if (!powered_) PowerOn();
        for (std::uint64_t i = 0; i < instructions; ++i) StepOne();
    }

    void RunUntil(Tick tick) override {
        if (!powered_) PowerOn();
        if (tick <= now_) {
            scheduler_.SetNow(now_);
            scheduler_.RunDue();
            return;
        }
        while (now_ < tick) {
            StepOne();
        }
    }

    void SetInput(const InputState& in) override {
        MachineInput mi;
        mi.p1 = ControllerFromBits(in.p1_buttons);
        mi.p2 = ControllerFromBits(in.p2_buttons);
        for (int i = 0; i < 8; ++i) mi.keyboard.rows[i] = in.keyboard[i];
        mi.reset = in.reset;
        input_ = mi;
    }

    void SetMachineInput(const MachineInput& in) override { input_ = in; }

    bool ScheduleInput(const InputTransition& transition) override {
        if (transition.tick < now_ || transition.tick < 0) return false;
        const std::uint32_t payload =
            static_cast<std::uint32_t>(transition.target) |
            (static_cast<std::uint32_t>(transition.index & 0x07) << 2) |
            (static_cast<std::uint32_t>(transition.value) << 5);
        scheduler_.Schedule(transition.tick, EventPriority::Input,
                            EventKind::InputTransition,
                            HandlerId::InputTransition, payload);
        return true;
    }

    bool SchedulePinTransition(Tick tick, bool t1_pin, bool level) override {
        if (tick < now_ || tick < 0) return false;
        const std::uint32_t payload =
            (t1_pin ? 0x80000000u : 0u) | (level ? 1u : 0u);
        scheduler_.Schedule(tick, EventPriority::Input, EventKind::TestTransition,
                            HandlerId::TestTransition, payload);
        return true;
    }

    void SetPinOverrides(bool t0_enabled, bool t0_level, bool t1_enabled,
                         bool t1_level) override {
        bus_.SetT0Override(t0_enabled, t0_level);
        bus_.SetT1Override(t1_enabled, t1_level);
    }

    const Framebuffer& GetFramebuffer() const override { return vdc_.Framebuffer(); }

    MachineInfo GetInfo() const override {
        MachineInfo info;
        info.bios_name = bios_name_;
        info.cartridge_name = cart_.info.name;
        info.active_region = profile_.region;
        info.cartridge_crc = cart_.info.crc32;
        info.cartridge_size = cart_.info.size;
        info.cartridge_kind = CartridgeKindName(cart_.info.kind);
        info.video_width = profile_.visible_w;
        info.video_height = profile_.visible_h;
        const double pixels = static_cast<double>(profile_.htotal) *
                              static_cast<double>(profile_.vtotal);
        info.frame_rate = pixels > 0.0
                              ? profile_.pixel_hz.ToDouble() / pixels
                              : 0.0;
        info.frame_ticks = profile_.FrameTicks();
        info.cpu_hz_numerator = profile_.cpu_hz.num;
        info.cpu_hz_denominator = profile_.cpu_hz.den;
        return info;
    }

    bool SaveState(std::vector<std::uint8_t>& out) const override {
        SnapshotWriter w;
        w.u32(kSnapshotMagic);
        w.u32(kSnapshotVersion);
        // Profile signature: a snapshot is only valid for the timing model
        // that produced it.
        w.u32(static_cast<std::uint32_t>(profile_.htotal));
        w.u32(static_cast<std::uint32_t>(profile_.vtotal));
        w.u32(static_cast<std::uint32_t>(profile_.visible_h));
        w.u32(static_cast<std::uint32_t>(profile_.pixel_ticks));
        w.u64(static_cast<std::uint64_t>(profile_.cpu_hz.num));
        w.u64(static_cast<std::uint64_t>(profile_.cpu_hz.den));
        w.u8(static_cast<std::uint8_t>(profile_.region));
        w.u64(static_cast<std::uint64_t>(now_));
        w.u64(frame_count_);
        w.u8(powered_ ? 1 : 0);

        const std::uint32_t bios_crc =
            bios_.empty() ? 0 : Crc32(bios_.data(), bios_.size());
        w.u32(bios_crc);
        w.u32(static_cast<std::uint32_t>(bios_.size()));
        w.u32(cart_.info.crc32);
        w.u32(static_cast<std::uint32_t>(cart_.rom.size()));
        w.u8(static_cast<std::uint8_t>(cart_.info.kind));

        w.raw(input_.keyboard.rows, sizeof(input_.keyboard.rows));
        w.u8(ControllerBits(input_.p1));
        w.u8(ControllerBits(input_.p2));
        w.u8(input_.reset ? 1 : 0);
        w.u8(last_irq_ ? 1 : 0);
        w.u8(last_t0_ ? 1 : 0);
        w.u8(last_t1_ ? 1 : 0);

        std::vector<std::uint8_t> cpu;
        cpu_.SaveState(cpu);
        w.u32(static_cast<std::uint32_t>(cpu.size()));
        w.raw(cpu.data(), cpu.size());

        scheduler_.SaveState(w);
        bus_.SaveState(w);
        w.u8(mapper_ ? 1 : 0);
        if (mapper_) mapper_->Snapshot(w);
        vdc_.SaveState(w);
        w.u8(config_.vpp_enabled ? 1 : 0);
        if (config_.vpp_enabled) vpp_.SaveState(w);
        audio_.SaveState(w);
        w.u32(0x454E4432u); // "END2"
        out = std::move(w.bytes);
        return true;
    }

    bool LoadState(const std::uint8_t* data, std::size_t size) override {
        if (!data || size < 8) return false;
        SnapshotReader r{data, size, 0, true};
        if (r.u32() != kSnapshotMagic) return false;
        if (r.u32() != kSnapshotVersion) return false;

        const std::uint32_t saved_htotal = r.u32();
        const std::uint32_t saved_vtotal = r.u32();
        const std::uint32_t saved_visible_h = r.u32();
        const std::uint32_t saved_pixel_ticks = r.u32();
        const std::int64_t saved_cpu_num = static_cast<std::int64_t>(r.u64());
        const std::int64_t saved_cpu_den = static_cast<std::int64_t>(r.u64());
        if (saved_htotal != static_cast<std::uint32_t>(profile_.htotal) ||
            saved_vtotal != static_cast<std::uint32_t>(profile_.vtotal) ||
            saved_visible_h != static_cast<std::uint32_t>(profile_.visible_h) ||
            saved_pixel_ticks != static_cast<std::uint32_t>(profile_.pixel_ticks) ||
            saved_cpu_num != profile_.cpu_hz.num ||
            saved_cpu_den != profile_.cpu_hz.den)
            return false;

        const Region saved_region = static_cast<Region>(r.u8());
        const Tick saved_now = static_cast<Tick>(r.u64());
        const std::uint64_t saved_frames = r.u64();
        const bool saved_powered = r.u8() != 0;

        const std::uint32_t bios_crc = r.u32();
        const std::uint32_t bios_size = r.u32();
        const std::uint32_t cart_crc = r.u32();
        const std::uint32_t cart_size = r.u32();
        const CartridgeKind cart_kind =
            static_cast<CartridgeKind>(r.u8());

        if (saved_region != profile_.region) return false;
        const std::uint32_t current_bios_crc =
            bios_.empty() ? 0 : Crc32(bios_.data(), bios_.size());
        if (bios_crc != current_bios_crc ||
            bios_size != bios_.size())
            return false;
        if (cart_crc != cart_.info.crc32 ||
            cart_size != cart_.rom.size() ||
            cart_kind != cart_.info.kind)
            return false;

        r.raw(input_.keyboard.rows, sizeof(input_.keyboard.rows));
        input_.p1 = ControllerFromBits(r.u8());
        input_.p2 = ControllerFromBits(r.u8());
        input_.reset = r.u8() != 0;
        last_irq_ = r.u8() != 0;
        last_t0_ = r.u8() != 0;
        last_t1_ = r.u8() != 0;

        const std::uint32_t cpu_size = r.u32();
        if (!r.ok || cpu_size > 4096) return false;
        std::vector<std::uint8_t> cpu(cpu_size);
        r.raw(cpu.data(), cpu.size());
        if (!r.ok || !cpu_.LoadState(cpu.data(), cpu.size())) return false;

        if (!scheduler_.LoadState(r)) return false;
        if (!bus_.LoadState(r)) return false;

        const bool has_mapper = r.u8() != 0;
        if (has_mapper) {
            if (!mapper_ || !mapper_->Restore(r)) return false;
        } else if (mapper_) {
            return false;
        }
        if (!vdc_.LoadState(r)) return false;
        const bool has_vpp = r.u8() != 0;
        if (has_vpp != config_.vpp_enabled) return false;
        if (has_vpp && !vpp_.LoadState(r)) return false;
        if (!audio_.LoadState(r)) return false;
        if (r.u32() != 0x454E4432u) return false;
        if (!r.ok) return false;

        now_ = saved_now;
        frame_count_ = saved_frames;
        powered_ = saved_powered;
        scheduler_.SetNow(now_);
        vdc_.SetTick(now_);
        bus_.SetAccessContext(now_, cpu_.State().pc);
        SyncCpuInputs();
        return true;
    }

    Tick Now() const override { return now_; }
    const HardwareProfile& Profile() const override { return profile_; }
    const Mcs48State& CpuState() const override { return cpu_.State(); }
    const Scheduler& GetScheduler() const override { return scheduler_; }
    const MachineBus& Bus() const override { return bus_; }
    const Cartridge& Cart() const override { return cart_; }
    const Mapper* MapperState() const override { return mapper_.get(); }
    const o2em::ng::Vdc& Vdc() const override { return vdc_; }
    const o2em::ng::Vpp& Vpp() const override { return vpp_; }
    const o2em::ng::AudioDevice& Audio() const override { return audio_; }
    BusTrace& MutableTrace() override { return trace_; }
    const BusTrace& Trace() const override { return trace_; }

private:
    void SyncCpuInputs() {
        cpu_.SetIrqLevel(bus_.IrqLine());
        cpu_.SetT0(bus_.T0());
        cpu_.SetT1(bus_.T1());
    }

    StepResult StepOne() {
        scheduler_.SetNow(now_);
        scheduler_.RunDue();
        SyncCpuInputs();
        bus_.SetAccessContext(now_, cpu_.State().pc);
        const StepResult result = cpu_.Step();
        if (special_) {
            const unsigned frame_cycles = static_cast<unsigned>(
                profile_.FrameTicks() / kTicksPerCpuCycle);
            special_->RunCpuCycles(result.cycles, frame_cycles);
        }
        now_ += CpuCyclesToTicks(result.cycles);
        scheduler_.SetNow(now_);
        scheduler_.RunDue();
        // Finish the raster for the completed instruction. Scheduler events
        // above may have advanced the device to their exact event tick; this
        // advances it to the post-instruction tick (update-on-write is handled
        // by Vdc::Read/Write before each bus mutation).
        vdc_.AdvanceTo(now_);
        bus_.SetAccessContext(now_, cpu_.State().pc);
        UpdateLineTraces();
        return result;
    }

    void UpdateLineTraces() {
        const bool irq = bus_.IrqLine();
        if (irq != last_irq_) {
            last_irq_ = irq;
            bus_.RecordLineTransition(BusOp::IrqTransition, irq ? 1 : 0);
        }
        const bool t0 = bus_.T0();
        if (t0 != last_t0_) {
            last_t0_ = t0;
            bus_.RecordLineTransition(BusOp::T0Transition, t0 ? 1 : 0);
        }
        const bool t1 = bus_.T1();
        if (t1 != last_t1_) {
            last_t1_ = t1;
            bus_.RecordLineTransition(BusOp::T1Transition, t1 ? 1 : 0);
        }
    }

    void ApplyInputPayload(std::uint32_t payload) {
        const InputTarget target = static_cast<InputTarget>(payload & 0x03);
        const std::uint8_t index = static_cast<std::uint8_t>((payload >> 2) & 0x07);
        const std::uint8_t value = static_cast<std::uint8_t>((payload >> 5) & 0xFF);
        switch (target) {
        case InputTarget::Controller1:
            input_.p1 = ControllerFromBits(value);
            break;
        case InputTarget::Controller2:
            input_.p2 = ControllerFromBits(value);
            break;
        case InputTarget::KeyboardRow:
            input_.keyboard.rows[index] = value;
            break;
        case InputTarget::ResetButton:
            input_.reset = value != 0;
            if (value != 0) Reset();
            break;
        }
    }

    MachineConfig config_;
    MachineHost* host_ = nullptr;
    HardwareProfile profile_{};
    Scheduler scheduler_{};
    o2em::ng::Vdc vdc_{};
    o2em::ng::Vpp vpp_{};
    o2em::ng::AudioDevice audio_{};
    std::vector<std::int16_t> audio_frame_{};
    Cartridge cart_{};
    std::vector<std::uint8_t> bios_;
    std::string bios_name_;
    std::unique_ptr<Mapper> mapper_;
    std::unique_ptr<SpecialHardware> special_;
    MachineInput input_{};
    BusTrace trace_{};
    MachineBus bus_{};
    Mcs48Cpu cpu_{bus_};
    Tick now_ = 0;
    std::uint64_t frame_count_ = 0;
    bool powered_ = false;
    bool last_irq_ = false;
    bool last_t0_ = false;
    bool last_t1_ = false;
};

} // namespace

std::unique_ptr<Machine> Machine::Create(const MachineConfig& config,
                                         MachineHost& host) {
    return std::make_unique<NgMachine>(config, host);
}

} // namespace o2em::ng
