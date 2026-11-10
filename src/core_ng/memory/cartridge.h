#pragma once
// O2EM-NG cartridge image and mapper selection.
//
// CRC is identity/diagnostics only. It never selects timing, video behaviour
// or title-specific workarounds. Mapper selection is a pure function of the
// image size/layout, matching documented cartridge hardware.

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace o2em::ng {

enum class CartridgeKind {
    None = 0,
    Normal2K,
    Normal4K,
    Normal8K,
    Normal32K,
    ThreeK3K,
    ThreeK6K,
    ThreeK12K,
    Rally16K,
    FourInOne4K,
    Unsupported
};

const char* CartridgeKindName(CartridgeKind kind);

struct CartridgeInfo {
    std::string path;
    std::string name;
    std::uint32_t crc32 = 0;
    std::size_t size = 0;
    CartridgeKind kind = CartridgeKind::None;
    // Cartridge B pin (Videopac+ colour-key/decoder mask). Hardware metadata.
    bool b_pin = false;
};

class Cartridge {
public:
    CartridgeInfo info;
    std::vector<std::uint8_t> rom;

    void Clear();
    bool Load(std::vector<std::uint8_t> bytes, std::string name = {},
              std::string path = {});
    bool LoadFile(const std::string& path);
    bool Empty() const { return rom.empty(); }
};

std::uint32_t Crc32(const std::uint8_t* data, std::size_t size);

} // namespace o2em::ng
