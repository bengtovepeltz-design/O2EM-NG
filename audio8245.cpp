#include "audio8245.h"

namespace
{
constexpr std::uint32_t kShiftRegisterMask = 0x00FFFFFFu;
}

void Audio8245::Reset() noexcept
{
    shiftRegister_ = 0;
    pendingShiftRegister_ = 0;
    control_ = 0;
    noiseLfsr_ = 0xACE1u;
    noiseOutputBit_ = 0;
    hblankPrescaler_ = 0;
    shiftCount_ = 0;
    writePending_ = false;
    shiftPending_ = false;
    interruptPending_ = false;
}

void Audio8245::LoadState(std::uint8_t data0, std::uint8_t data1,
                          std::uint8_t data2, std::uint8_t control) noexcept
{
    const std::uint32_t value =
        (static_cast<std::uint32_t>(data0) << 16) |
        (static_cast<std::uint32_t>(data1) << 8) |
        static_cast<std::uint32_t>(data2);

    shiftRegister_ = value & kShiftRegisterMask;
    pendingShiftRegister_ = shiftRegister_;
    control_ = control;
    writePending_ = false;
    shiftPending_ = false;
    shiftCount_ = 0;
    interruptPending_ = false;
}

void Audio8245::WriteRegister(std::uint16_t address, std::uint8_t value) noexcept
{
    // A7-A9 are assembled in a separate CPU-visible latch. The active serial
    // register is updated atomically at HBLANK, rather than changing one byte
    // at a time while a 24-bit sequence may still be shifting.
    switch (address & 0x00FFu)
    {
    case 0xA7u:
        pendingShiftRegister_ =
            (pendingShiftRegister_ & 0x0000FFFFu) |
            (static_cast<std::uint32_t>(value) << 16);
        break;

    case 0xA8u:
        pendingShiftRegister_ =
            (pendingShiftRegister_ & 0x00FF00FFu) |
            (static_cast<std::uint32_t>(value) << 8);
        break;

    case 0xA9u:
        pendingShiftRegister_ =
            (pendingShiftRegister_ & 0x00FFFF00u) |
            static_cast<std::uint32_t>(value);
        break;

    case 0xAAu:
        WriteControl(value);
        return;

    default:
        return;
    }

    pendingShiftRegister_ &= kShiftRegisterMask;
    writePending_ = true;
}

void Audio8245::WriteControl(std::uint8_t value) noexcept
{
    control_ = value;
}

void Audio8245::CommitPendingWrite() noexcept
{
    shiftRegister_ = pendingShiftRegister_ & kShiftRegisterMask;
    writePending_ = false;
    shiftCount_ = 0;
}

std::uint8_t Audio8245::NextNoiseBit() noexcept
{
    const std::uint16_t feedback = static_cast<std::uint16_t>(
        ((noiseLfsr_ >> 0) ^ (noiseLfsr_ >> 2) ^
         (noiseLfsr_ >> 3) ^ (noiseLfsr_ >> 5)) & 1u);

    noiseLfsr_ = static_cast<std::uint16_t>(
        (noiseLfsr_ >> 1) | (feedback << 15));

    return static_cast<std::uint8_t>(noiseLfsr_ & 1u);
}

void Audio8245::HBlank() noexcept
{
    ++hblankPrescaler_;

    const std::uint32_t prescalerMask = FastClock() ? 0x03u : 0x0Fu;
    if ((hblankPrescaler_ & prescalerMask) == 0)
        shiftPending_ = true;

    // A register write takes priority at the next HBLANK. A previously queued
    // shift remains pending and is serviced by a later HBLANK, matching the
    // write/pending sequencing used by the reference implementation.
    if (writePending_)
    {
        CommitPendingWrite();
        return;
    }

    if (!shiftPending_ || !Enabled())
        return;

    shiftPending_ = false;

    const std::uint32_t outgoing = shiftRegister_ & 1u;
    shiftRegister_ >>= 1;

    if (Recirculate())
        shiftRegister_ |= outgoing << 23;

    shiftRegister_ &= kShiftRegisterMask;
    noiseOutputBit_ = NoiseEnabled() ? NextNoiseBit() : 0;

    ++shiftCount_;
    if (shiftCount_ >= 24)
    {
        shiftCount_ = 0;
        interruptPending_ = true;
    }
}

std::uint8_t Audio8245::OutputBit() const noexcept
{
    const std::uint8_t serial = static_cast<std::uint8_t>(shiftRegister_ & 1u);
    return NoiseEnabled()
        ? static_cast<std::uint8_t>(serial ^ noiseOutputBit_)
        : serial;
}

bool Audio8245::ConsumeInterrupt() noexcept
{
    const bool pending = interruptPending_;
    interruptPending_ = false;
    return pending;
}
