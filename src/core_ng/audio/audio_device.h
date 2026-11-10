#pragma once
// O2EM-NG owned i8244/i8245 sound device (NG-6).
//
// The Intel 8244/8245 sound hardware is a 24-bit serial shift register with
// selectable 4/16-line HBLANK prescaler, output enable, noise feedback and a
// 16-step volume duty.  The device is clocked only by authoritative NG ticks:
// machine writes are timestamped, HBLANK events come from the VDC scanline
// engine, and PCM is generated from exact rational tick/sample phase.
//
// There is no SDL, host audio queue, legacy global, frame-boundary reset or
// title-specific behaviour in this device.

#include <cstdint>
#include <vector>

#include "../state.h"
#include "../timebase.h"

namespace o2em::ng {

class AudioDevice {
public:
    // Configure for a machine profile and a canonical PCM rate (normally
    // 48000 Hz). Must be called before PowerOn/Reset.
    void Configure(const HardwareProfile& profile, int sample_rate = 48000);

    // Hardware reset at the supplied authoritative tick. PowerOn uses 0.
    void Reset(Tick now);
    void PowerOn() { Reset(0); }

    // Advance PCM generation to `tick` with the current hardware state.
    void AdvanceTo(Tick tick);

    // HBLANK event from the VDC. Advances audio to the exact event tick, then
    // clocks the prescaler/shift register.
    void Hblank(Tick tick, bool sound_irq_enable);

    // CPU-visible sound registers A7, A8, A9, AA. The write is applied at
    // exactly `tick`; the old state generates audio only before it.
    void WriteRegister(std::uint8_t reg, std::uint8_t value, Tick tick);
    std::uint8_t ReadRegister(std::uint8_t reg) const;

    std::uint8_t SoundRegister() const { return sound_; }
    std::uint8_t ShiftHigh() const { return shift1_; }
    std::uint8_t ShiftMid() const { return shift2_; }
    std::uint8_t ShiftLow() const { return shift3_; }
    bool ShiftOutput() const { return sh_output_ != 0; }
    bool IrqLine() const { return irq_line_; }
    void ClearIrq() { irq_line_ = false; }
    Tick LastTick() const { return last_tick_; }
    Tick LastWriteTick() const { return last_write_tick_; }
    Tick NextSampleTick() const;
    Tick LastShiftTick() const { return last_shift_tick_; }
    std::uint8_t Prescaler() const { return sh_prescaler_; }
    std::uint8_t ShiftCount() const { return sh_count_; }
    bool Pending() const { return pending_; }
    bool Written() const { return written_; }
    int SampleRate() const { return sample_rate_; }

    // Canonical PCM produced since the last Drain. Signed 16-bit mono.
    const std::vector<std::int16_t>& Pcm() const { return pcm_; }
    void Drain(std::vector<std::int16_t>& out);
    std::uint64_t PcmHash() const;

    bool SaveState(SnapshotWriter& w) const;
    bool LoadState(SnapshotReader& r);

private:
    void GenerateTicks(std::int64_t ticks, std::int32_t amplitude);
    void Shift();
    std::int32_t CurrentAmplitude() const;
    void EmitSample(std::int32_t value);

    HardwareProfile profile_{};
    int sample_rate_ = 48000;
    std::int64_t tick_hz_num_ = 1;
    std::int64_t tick_hz_den_ = 1;
    std::int64_t tick_step_ = 1; // sample_rate * tick_hz_den_

    std::uint8_t sound_ = 0;
    std::uint8_t shift1_ = 0; // 0xA7 high byte
    std::uint8_t shift2_ = 0; // 0xA8 middle byte
    std::uint8_t shift3_ = 0; // 0xA9 low byte / output bit 0
    std::uint8_t sh_prescaler_ = 0;
    std::uint8_t sh_count_ = 0;
    bool pending_ = false;
    bool written_ = false;
    std::uint8_t sh_output_ = 0;
    bool irq_line_ = false;

    Tick last_tick_ = 0;
    Tick last_write_tick_ = 0;
    Tick last_shift_tick_ = 0;
    std::int64_t phase_ = 0;        // ticks * sample_rate * tick_hz_den
    std::int64_t sample_accum_ = 0; // amplitude * ticks in current sample
    std::int64_t sample_ticks_ = 0;
    std::vector<std::int16_t> pcm_;
};

} // namespace o2em::ng
