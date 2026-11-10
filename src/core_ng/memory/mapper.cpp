#include "mapper.h"

#include <algorithm>

namespace o2em::ng {

namespace {

std::uint32_t BitFloorMask(std::size_t size) {
    if (size == 0) return 0;
    std::uint32_t mask = 1;
    while (mask <= size / 2) mask <<= 1;
    return mask - 1;
}

// Window offsets used by the standard/rally wiring:
//   read_rom04(offset) = byte at 0x400+offset
//   read_rom0c(offset) = read_rom04(offset + 0x400)
bool WindowOffset(std::uint16_t address, std::uint32_t& offset) {
    if (address >= 0x0400 && address <= 0x0BFF) {
        offset = static_cast<std::uint32_t>(address - 0x0400);
        return true;
    }
    if (address >= 0x0C00 && address <= 0x0FFF) {
        offset = static_cast<std::uint32_t>(address - 0x0C00) + 0x0400;
        return true;
    }
    return false;
}

} // namespace

void StandardMapper::Reset(const Cartridge& cartridge) {
    cart_ = &cartridge;
    mask_ = BitFloorMask(cartridge.rom.size());
    bank_ = 0;
}

std::uint8_t StandardMapper::ReadProgram(std::uint16_t address) {
    std::uint32_t offset = 0;
    if (!cart_ || !WindowOffset(address, offset)) return 0xFF;
    offset = (offset + std::uint32_t(bank_) * 0x0800u) & mask_;
    if (offset >= cart_->rom.size()) return 0xFF;
    return cart_->rom[offset];
}

void StandardMapper::WriteP1(std::uint8_t p1) {
    bank_ = static_cast<std::uint8_t>(p1 & 0x03);
}

void StandardMapper::Snapshot(SnapshotWriter& w) const {
    w.u32(0x4D415031u); // "MAP1"
    w.u8(bank_);
}

bool StandardMapper::Restore(SnapshotReader& r) {
    if (r.u32() != 0x4D415031u) return false;
    bank_ = r.u8();
    return r.ok && bank_ <= 3;
}

void ThreeKMapper::Reset(const Cartridge& cartridge) {
    cart_ = &cartridge;
    const std::size_t pages = cartridge.rom.size() / 0x0C00u;
    bank_mask_ = static_cast<std::uint8_t>(pages > 0 ? pages - 1 : 0);
    bank_ = 0;
}

std::uint8_t ThreeKMapper::ReadProgram(std::uint16_t address) {
    if (!cart_) return 0xFF;
    std::uint32_t offset = 0;
    if (address >= 0x0400 && address <= 0x0BFF)
        offset = static_cast<std::uint32_t>(address - 0x0400);
    else if (address >= 0x0C00 && address <= 0x0FFF)
        offset = static_cast<std::uint32_t>(address - 0x0C00) + 0x0800u;
    else
        return 0xFF;

    const std::uint32_t index =
        std::uint32_t(bank_ & bank_mask_) * 0x0C00u + offset;
    if (index >= cart_->rom.size()) return 0xFF;
    return cart_->rom[index];
}

void ThreeKMapper::WriteP1(std::uint8_t p1) {
    bank_ = static_cast<std::uint8_t>(p1 & bank_mask_);
}

void ThreeKMapper::Snapshot(SnapshotWriter& w) const {
    w.u32(0x4D415033u); // "MAP3"
    w.u8(bank_);
}

bool ThreeKMapper::Restore(SnapshotReader& r) {
    if (r.u32() != 0x4D415033u) return false;
    bank_ = r.u8();
    return r.ok;
}

void RallyMapper::Reset(const Cartridge& cartridge) {
    cart_ = &cartridge;
    mask_ = BitFloorMask(cartridge.rom.size());
    control_ = 0;
    bank_ = 0;
}

std::uint8_t RallyMapper::ReadProgram(std::uint16_t address) {
    std::uint32_t offset = 0;
    if (!cart_ || !WindowOffset(address, offset)) return 0xFF;
    const std::uint8_t bank = (control_ & 0x01) ? 0xFFu : bank_;
    const std::uint32_t index =
        (offset + std::uint32_t(bank) * 0x0800u) & mask_;
    if (index >= cart_->rom.size()) return 0xFF;
    return cart_->rom[index];
}

void RallyMapper::WriteP1(std::uint8_t p1) {
    control_ = static_cast<std::uint8_t>(p1 & 0x13);
}

std::uint8_t RallyMapper::IoRead(std::uint8_t) {
    return 0xFF;
}

void RallyMapper::IoWrite(std::uint8_t address, std::uint8_t value) {
    if ((address & 0x80) && !(control_ & 0x10))
        bank_ = value;
}

void RallyMapper::Snapshot(SnapshotWriter& w) const {
    w.u32(0x4D415052u); // "MAPR"
    w.u8(control_);
    w.u8(bank_);
}

bool RallyMapper::Restore(SnapshotReader& r) {
    if (r.u32() != 0x4D415052u) return false;
    control_ = r.u8();
    bank_ = r.u8();
    return r.ok;
}

void FourInOneMapper::Reset(const Cartridge& cartridge) {
    cart_ = &cartridge;
    control_ = 0;
    bank_ = 0;
}

std::uint8_t FourInOneMapper::ReadProgram(std::uint16_t address) {
    // Fixed program ROM is the upper 3KB of the 4KB image. MAME's
    // read_rom04/read_rom0c both resolve to rom[address] for this PCB.
    if (!cart_) return 0xFF;
    if (address < 0x0400 || address > 0x0FFF) return 0xFF;
    if (address >= cart_->rom.size()) return 0xFF;
    return cart_->rom[address];
}

void FourInOneMapper::WriteP1(std::uint8_t p1) {
    control_ = p1;
}

void FourInOneMapper::WriteP2(std::uint8_t p2) {
    bank_ = static_cast<std::uint8_t>(p2 & 0x0F);
}

std::uint8_t FourInOneMapper::IoRead(std::uint8_t address) {
    // Data ROM banked by P2; enabled while P1 bit1 is set.
    if (!(control_ & 0x02) || !cart_) return 0xFF;
    const std::uint32_t index = (std::uint32_t(bank_) << 8) |
                                std::uint32_t(address);
    if (index >= cart_->rom.size()) return 0xFF;
    return cart_->rom[index];
}

void FourInOneMapper::Snapshot(SnapshotWriter& w) const {
    w.u32(0x4D415034u); // "MAP4"
    w.u8(control_);
    w.u8(bank_);
}

bool FourInOneMapper::Restore(SnapshotReader& r) {
    if (r.u32() != 0x4D415034u) return false;
    control_ = r.u8();
    bank_ = r.u8();
    return r.ok;
}

std::unique_ptr<Mapper> MakeMapper(const Cartridge& cartridge) {
    switch (cartridge.info.kind) {
    case CartridgeKind::Normal2K:
    case CartridgeKind::Normal4K:
    case CartridgeKind::Normal8K:
    case CartridgeKind::Normal32K:
        return std::make_unique<StandardMapper>();
    case CartridgeKind::ThreeK3K:
    case CartridgeKind::ThreeK6K:
    case CartridgeKind::ThreeK12K:
        return std::make_unique<ThreeKMapper>();
    case CartridgeKind::Rally16K:
        return std::make_unique<RallyMapper>();
    case CartridgeKind::FourInOne4K:
        return std::make_unique<FourInOneMapper>();
    case CartridgeKind::None:
    case CartridgeKind::Unsupported:
    default:
        return nullptr;
    }
}

} // namespace o2em::ng
