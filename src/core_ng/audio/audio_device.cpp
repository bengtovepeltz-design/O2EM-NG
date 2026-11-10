#include "audio_device.h"

#include <algorithm>
#include <cstring>

namespace o2em::ng {

namespace {
constexpr std::uint32_t kSnapshotMagic = 0x36445541u; // "AUD6"
constexpr std::uint32_t kSnapshotVersion = 1u;

// MAME routes the i824x sound output at 0.5 and applies a 16-step volume
// duty. The audible low-pass average is 0.5 * volume / 16. Keep the logical
// amplitude in signed 16-bit full scale, independent of host format.
constexpr std::int32_t kMaxAmplitude = 16384;

std::uint64_t Fnv1a(const std::uint8_t* data, std::size_t n) {
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < n; ++i) {
        h ^= data[i];
        h *= 1099511628211ull;
    }
    return h;
}
} // namespace

void AudioDevice::Configure(const HardwareProfile& profile, int sample_rate) {
    profile_ = profile;
    sample_rate_ = sample_rate > 0 ? sample_rate : 48000;
    // One 8048 machine cycle is 15 oscillator periods and 180 NG ticks, so
    // the NG tick rate is cpu_hz * 12 (cpu_hz is the oscillator frequency).
    tick_hz_num_ = profile_.cpu_hz.num * 12;
    tick_hz_den_ = profile_.cpu_hz.den > 0 ? profile_.cpu_hz.den : 1;
    if (tick_hz_num_ <= 0) tick_hz_num_ = 1;
    tick_step_ = static_cast<std::int64_t>(sample_rate_) * tick_hz_den_;
    if (tick_step_ <= 0) tick_step_ = 1;
    Reset(0);
}

void AudioDevice::Reset(Tick now) {
    sound_ = 0;
    shift1_ = shift2_ = shift3_ = 0;
    sh_prescaler_ = 0;
    sh_count_ = 0;
    pending_ = false;
    written_ = false;
    sh_output_ = 0;
    irq_line_ = false;
    last_tick_ = now < 0 ? 0 : now;
    last_write_tick_ = last_tick_;
    last_shift_tick_ = last_tick_;
    phase_ = 0;
    sample_accum_ = 0;
    sample_ticks_ = 0;
    pcm_.clear();
}

std::int32_t AudioDevice::CurrentAmplitude() const {
    if (!(sound_ & 0x80) || !sh_output_) return 0;
    return (kMaxAmplitude * (sound_ & 0x0F)) / 16;
}

void AudioDevice::EmitSample(std::int32_t value) {
    if (value > 32767) value = 32767;
    if (value < -32768) value = -32768;
    pcm_.push_back(static_cast<std::int16_t>(value));
}

void AudioDevice::GenerateTicks(std::int64_t ticks, std::int32_t amplitude) {
    if (ticks <= 0) return;
    std::int64_t remaining = ticks;
    while (remaining > 0) {
        std::int64_t until =
            (tick_hz_num_ - phase_ + tick_step_ - 1) / tick_step_;
        if (until <= 0) until = 1;
        const std::int64_t take = std::min(remaining, until);
        sample_accum_ += static_cast<std::int64_t>(amplitude) * take;
        sample_ticks_ += take;
        phase_ += take * tick_step_;
        remaining -= take;
        if (phase_ >= tick_hz_num_) {
            const std::int32_t value = sample_ticks_ > 0
                ? static_cast<std::int32_t>(
                      (sample_accum_ + sample_ticks_ / 2) / sample_ticks_)
                : 0;
            EmitSample(value);
            sample_accum_ = 0;
            sample_ticks_ = 0;
            phase_ -= tick_hz_num_;
        }
    }
}

Tick AudioDevice::NextSampleTick() const {
    const std::int64_t until =
        (tick_hz_num_ - phase_ + tick_step_ - 1) / tick_step_;
    return last_tick_ + (until > 0 ? until : 1);
}

void AudioDevice::AdvanceTo(Tick tick) {
    if (tick < 0) tick = 0;
    if (tick <= last_tick_) {
        last_tick_ = tick;
        return;
    }
    GenerateTicks(tick - last_tick_, CurrentAmplitude());
    last_tick_ = tick;
}

void AudioDevice::Shift() {
    std::uint32_t signal = static_cast<std::uint32_t>(shift3_) |
                           (static_cast<std::uint32_t>(shift2_) << 8) |
                           (static_cast<std::uint32_t>(shift1_) << 16);
    sh_output_ = static_cast<std::uint8_t>(signal & 1u);
    int feedback = sh_output_;
    signal >>= 1;
    // Noise tap on pre-shift bits 0 and 5, fed back into bit 15.
    if (sound_ & 0x10) {
        feedback ^= static_cast<int>((signal >> 4) & 1u);
        signal = (signal & ~0x8000u) | (static_cast<std::uint32_t>(feedback) << 15);
    }
    // Loop sound: the output bit is fed back into bit 23.
    signal |= static_cast<std::uint32_t>(feedback) << 23;
    shift3_ = static_cast<std::uint8_t>(signal & 0xFF);
    shift2_ = static_cast<std::uint8_t>((signal >> 8) & 0xFF);
    shift1_ = static_cast<std::uint8_t>((signal >> 16) & 0xFF);
}

void AudioDevice::Hblank(Tick tick, bool sound_irq_enable) {
    AdvanceTo(tick);
    ++sh_prescaler_;
    const std::uint8_t prescaler_mask = (sound_ & 0x20) ? 3 : 0x0F;
    if ((sh_prescaler_ & prescaler_mask) == 0) pending_ = true;

    if (pending_ && !written_) {
        pending_ = false;
        last_shift_tick_ = tick;
        Shift();
        if (++sh_count_ == 24) {
            sh_count_ = 0;
            if (sound_irq_enable) irq_line_ = true;
        }
    } else if (written_) {
        // The CPU loaded a new 24-bit pattern. MAME consumes the write latch
        // at this HBLANK and does not shift that event; the next prescaler
        // match shifts the freshly loaded pattern.
        sh_count_ = 0;
        written_ = false;
    }
}

void AudioDevice::WriteRegister(std::uint8_t reg, std::uint8_t value, Tick tick) {
    AdvanceTo(tick);
    last_write_tick_ = tick;
    switch (reg) {
    case 0xA7:
        shift1_ = value;
        written_ = true;
        break;
    case 0xA8:
        shift2_ = value;
        written_ = true;
        break;
    case 0xA9:
        shift3_ = value;
        written_ = true;
        break;
    case 0xAA:
        sound_ = static_cast<std::uint8_t>(value & ~0x40u);
        break;
    default:
        break;
    }
}

std::uint8_t AudioDevice::ReadRegister(std::uint8_t reg) const {
    switch (reg) {
    case 0xA7:
    case 0xA8:
    case 0xA9:
        return 0; // write-only shift-register bytes
    case 0xAA:
        return sound_;
    default:
        return 0;
    }
}

void AudioDevice::Drain(std::vector<std::int16_t>& out) {
    out.insert(out.end(), pcm_.begin(), pcm_.end());
    pcm_.clear();
}

std::uint64_t AudioDevice::PcmHash() const {
    return Fnv1a(reinterpret_cast<const std::uint8_t*>(pcm_.data()),
                 pcm_.size() * sizeof(std::int16_t));
}

bool AudioDevice::SaveState(SnapshotWriter& w) const {
    w.u32(kSnapshotMagic);
    w.u32(kSnapshotVersion);
    w.u32(static_cast<std::uint32_t>(sample_rate_));
    w.u64(static_cast<std::uint64_t>(tick_hz_num_));
    w.u64(static_cast<std::uint64_t>(tick_hz_den_));
    w.u8(sound_);
    w.u8(shift1_);
    w.u8(shift2_);
    w.u8(shift3_);
    w.u8(sh_prescaler_);
    w.u8(sh_count_);
    w.u8(pending_ ? 1 : 0);
    w.u8(written_ ? 1 : 0);
    w.u8(sh_output_);
    w.u8(irq_line_ ? 1 : 0);
    w.u64(static_cast<std::uint64_t>(last_tick_));
    w.u64(static_cast<std::uint64_t>(phase_));
    w.u64(static_cast<std::uint64_t>(sample_accum_));
    w.u64(static_cast<std::uint64_t>(sample_ticks_));
    w.u32(static_cast<std::uint32_t>(pcm_.size()));
    if (!pcm_.empty()) w.raw(pcm_.data(), pcm_.size() * sizeof(std::int16_t));
    return true;
}

bool AudioDevice::LoadState(SnapshotReader& r) {
    if (r.u32() != kSnapshotMagic) return false;
    if (r.u32() != kSnapshotVersion) return false;
    if (static_cast<int>(r.u32()) != sample_rate_) return false;
    if (static_cast<std::int64_t>(r.u64()) != tick_hz_num_) return false;
    if (static_cast<std::int64_t>(r.u64()) != tick_hz_den_) return false;
    sound_ = r.u8();
    shift1_ = r.u8();
    shift2_ = r.u8();
    shift3_ = r.u8();
    sh_prescaler_ = r.u8();
    sh_count_ = r.u8();
    pending_ = r.u8() != 0;
    written_ = r.u8() != 0;
    sh_output_ = r.u8();
    irq_line_ = r.u8() != 0;
    last_tick_ = static_cast<Tick>(r.u64());
    phase_ = static_cast<std::int64_t>(r.u64());
    sample_accum_ = static_cast<std::int64_t>(r.u64());
    sample_ticks_ = static_cast<std::int64_t>(r.u64());
    const std::uint32_t count = r.u32();
    if (count > 1u << 20) return false;
    pcm_.resize(count);
    if (count) r.raw(pcm_.data(), count * sizeof(std::int16_t));
    return r.ok;
}

} // namespace o2em::ng
