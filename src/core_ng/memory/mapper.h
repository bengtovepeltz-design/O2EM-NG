#pragma once
// O2EM-NG cartridge program-memory mappers.
//
// Program address ownership:
//   0x000-0x3FF  MCS-48 internal BIOS ROM (never cartridge)
//   0x400-0xBFF  cartridge 2KB window A
//   0xC00-0xFFF  cartridge 1KB window B (mapper-defined relationship to A)
//
// A mapper only sees the 0x400-0xFFF window. It never owns BIOS or internal
// CPU RAM. Mapper state is snapshotted as part of the machine.

#include <cstdint>
#include <memory>

#include "../state.h"
#include "cartridge.h"

namespace o2em::ng {

class Mapper {
public:
    virtual ~Mapper() = default;

    virtual const char* Name() const = 0;
    virtual void Reset(const Cartridge& cartridge) = 0;

    // address is in 0x400..0xFFF.
    virtual std::uint8_t ReadProgram(std::uint16_t address) = 0;

    // P1 output, already masked to the cartridge-visible bits (0x13).
    virtual void WriteP1(std::uint8_t p1) = 0;

    // P2 output low nibble (MAME passes m_p2 & 0x0f). Standard carts ignore it.
    virtual void WriteP2(std::uint8_t /*p2*/) {}

    // MOVX decode hooks. Standard cartridges do not claim these.
    virtual std::uint8_t IoRead(std::uint8_t /*address*/) { return 0xFF; }
    virtual void IoWrite(std::uint8_t /*address*/, std::uint8_t /*value*/) {}
    virtual std::uint8_t BusRead() { return 0xFF; }
    virtual void BusWrite(std::uint8_t /*value*/) {}
    virtual bool T0() const { return false; }

    virtual std::uint8_t Bank() const = 0;
    virtual std::uint8_t Control() const { return 0; }

    virtual void Snapshot(SnapshotWriter& w) const = 0;
    virtual bool Restore(SnapshotReader& r) = 0;
};

// 2KB/4KB/8KB standard cartridges. The selected 2KB bank is P1[1:0].
class StandardMapper final : public Mapper {
public:
    const char* Name() const override { return "standard"; }
    void Reset(const Cartridge&) override;
    std::uint8_t ReadProgram(std::uint16_t address) override;
    void WriteP1(std::uint8_t p1) override;
    std::uint8_t Bank() const override { return bank_; }
    void Snapshot(SnapshotWriter& w) const override;
    bool Restore(SnapshotReader& r) override;

private:
    const Cartridge* cart_ = nullptr;
    std::uint32_t mask_ = 0;
    std::uint8_t bank_ = 0;
};

// 3KB-page cartridges (e.g. 12KB freely distributed KTAA/VP55 images).
// P1[1:0] selects one of up to four 3KB pages.
class ThreeKMapper final : public Mapper {
public:
    const char* Name() const override { return "3k-page"; }
    void Reset(const Cartridge&) override;
    std::uint8_t ReadProgram(std::uint16_t address) override;
    void WriteP1(std::uint8_t p1) override;
    std::uint8_t Bank() const override { return bank_; }
    void Snapshot(SnapshotWriter& w) const override;
    bool Restore(SnapshotReader& r) override;

private:
    const Cartridge* cart_ = nullptr;
    std::uint8_t bank_mask_ = 0;
    std::uint8_t bank_ = 0;
};

// Videopac+ 55/58/59/60 latch mapper (MAME o2_rally).
// P10 high forces the last 8KB bank; P10 low selects the latched bank.
// A MOVX write to an address with A7 set updates the latch when P14 is low.
class RallyMapper final : public Mapper {
public:
    const char* Name() const override { return "rally"; }
    void Reset(const Cartridge&) override;
    std::uint8_t ReadProgram(std::uint16_t address) override;
    void WriteP1(std::uint8_t p1) override;
    std::uint8_t IoRead(std::uint8_t address) override;
    void IoWrite(std::uint8_t address, std::uint8_t value) override;
    std::uint8_t Bank() const override {
        return (control_ & 0x01) ? 0xFFu : bank_;
    }
    std::uint8_t Control() const override { return control_; }
    void Snapshot(SnapshotWriter& w) const override;
    bool Restore(SnapshotReader& r) override;

private:
    const Cartridge* cart_ = nullptr;
    std::uint32_t mask_ = 0;
    std::uint8_t control_ = 0;
    std::uint8_t bank_ = 0;
};

// Videopac 31/40 "4-in-1" cartridge PCB (MAME o2_4in1).
// The fixed program ROM is the upper 3KB (0x400..0xFFF); the lower 1KB is a
// data ROM selected by P2 and read through MOVX when P1 bit1 is set.
class FourInOneMapper final : public Mapper {
public:
    const char* Name() const override { return "4in1"; }
    void Reset(const Cartridge&) override;
    std::uint8_t ReadProgram(std::uint16_t address) override;
    void WriteP1(std::uint8_t p1) override;
    void WriteP2(std::uint8_t p2) override;
    std::uint8_t IoRead(std::uint8_t address) override;
    std::uint8_t Bank() const override { return bank_; }
    std::uint8_t Control() const override { return control_; }
    void Snapshot(SnapshotWriter& w) const override;
    bool Restore(SnapshotReader& r) override;

private:
    const Cartridge* cart_ = nullptr;
    std::uint8_t control_ = 0;
    std::uint8_t bank_ = 0;
};

std::unique_ptr<Mapper> MakeMapper(const Cartridge& cartridge);

} // namespace o2em::ng
