#pragma once

#include <cstdint>

// Standalone Intel 8244/8245 sound core.
// The module has no SDL, frontend or host-audio dependencies.
class Audio8245 final
{
public:
    static constexpr std::uint16_t RegisterData0 = 0x00A7;
    static constexpr std::uint16_t RegisterData1 = 0x00A8;
    static constexpr std::uint16_t RegisterData2 = 0x00A9;
    static constexpr std::uint16_t RegisterControl = 0x00AA;

    void Reset() noexcept;
    void LoadState(std::uint8_t data0, std::uint8_t data1,
                   std::uint8_t data2, std::uint8_t control) noexcept;

    void WriteRegister(std::uint16_t address, std::uint8_t value) noexcept;
    void WriteControl(std::uint8_t value) noexcept;
    void HBlank() noexcept;

    [[nodiscard]] std::uint8_t Control() const noexcept { return control_; }
    [[nodiscard]] std::uint32_t ShiftRegister() const noexcept { return shiftRegister_ & 0x00FFFFFFu; }
    [[nodiscard]] std::uint32_t PendingShiftRegister() const noexcept { return pendingShiftRegister_ & 0x00FFFFFFu; }
    [[nodiscard]] bool WritePending() const noexcept { return writePending_; }
    [[nodiscard]] bool Enabled() const noexcept { return (control_ & 0x80u) != 0; }
    [[nodiscard]] std::uint8_t Volume() const noexcept { return control_ & 0x0Fu; }
    [[nodiscard]] bool NoiseEnabled() const noexcept { return (control_ & 0x10u) != 0; }
    [[nodiscard]] bool FastClock() const noexcept { return (control_ & 0x20u) != 0; }
    [[nodiscard]] bool Recirculate() const noexcept { return (control_ & 0x40u) != 0; }
    [[nodiscard]] std::uint8_t OutputBit() const noexcept;
    [[nodiscard]] bool ConsumeInterrupt() noexcept;

private:
    [[nodiscard]] std::uint8_t NextNoiseBit() noexcept;
    void CommitPendingWrite() noexcept;

    std::uint32_t shiftRegister_ = 0;
    std::uint32_t pendingShiftRegister_ = 0;
    std::uint8_t control_ = 0;
    std::uint16_t noiseLfsr_ = 0xACE1u;
    std::uint8_t noiseOutputBit_ = 0;
    std::uint32_t hblankPrescaler_ = 0;
    std::uint8_t shiftCount_ = 0;
    bool writePending_ = false;
    bool shiftPending_ = false;
    bool interruptPending_ = false;
};
