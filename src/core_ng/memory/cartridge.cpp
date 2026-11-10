#include "cartridge.h"

#include <cstdio>
#include <utility>

namespace o2em::ng {

namespace {

std::uint32_t Crc32Update(std::uint32_t crc, const std::uint8_t* data,
                          std::size_t size) {
    crc = ~crc;
    for (std::size_t i = 0; i < size; ++i) {
        crc ^= data[i];
        for (int bit = 0; bit < 8; ++bit)
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}


bool HasBPin(std::uint32_t crc) {
    // Cartridge B pin tied high: MAME videopac.xml feature b_pin=1.
    // This is PCB hardware metadata, keyed by the cartridge CRC, never timing.
    switch (crc) {
    case 0x02743352u:
    case 0x06861A9Cu:
    case 0x0D2D721Du:
    case 0x0E039F31u:
    case 0x105231F8u:
    case 0x1931F5C5u:
    case 0x1C750349u:
    case 0x20E2F7BEu:
    case 0x2391C2FBu:
    case 0x26B0FF5Bu:
    case 0x2FCE9245u:
    case 0x3026E732u:
    case 0x3150A9BFu:
    case 0x38269D8Bu:
    case 0x3C949A57u:
    case 0x4CEC1404u:
    case 0x4E2CC6D3u:
    case 0x50AF9D45u:
    case 0x5987097Eu:
    case 0x68560DC7u:
    case 0x6FABCB7Eu:
    case 0x764894A1u:
    case 0x7C989567u:
    case 0x8E824B98u:
    case 0x939F0E4Cu:
    case 0x9585D511u:
    case 0x9BC04EBBu:
    case 0x9D2C45DCu:
    case 0x9D72D4E9u:
    case 0x9E42E766u:
    case 0xA75C42F8u:
    case 0xA8DBED92u:
    case 0xAC5C74E6u:
    case 0xB2F0F0B4u:
    case 0xB2FFB353u:
    case 0xB4735E6Bu:
    case 0xB5FB1416u:
    case 0xB72B1494u:
    case 0xB932D584u:
    case 0xBE11157Cu:
    case 0xC4134DF8u:
    case 0xCFCA6F39u:
    case 0xD94D4109u:
    case 0xDEE70B33u:
    case 0xE5D2CF23u:
    case 0xE5FB4260u:
    case 0xE8E985D4u:
    case 0xEE3EE642u:
        return true;
    default:
        return false;
    }
}

bool IsPowerOfTwo(std::size_t v) {
    return v != 0 && (v & (v - 1)) == 0;
}

} // namespace

std::uint32_t Crc32(const std::uint8_t* data, std::size_t size) {
    return Crc32Update(0, data, size);
}

const char* CartridgeKindName(CartridgeKind kind) {
    switch (kind) {
    case CartridgeKind::None: return "none";
    case CartridgeKind::Normal2K: return "normal-2k";
    case CartridgeKind::Normal4K: return "normal-4k";
    case CartridgeKind::Normal8K: return "normal-8k";
    case CartridgeKind::Normal32K: return "normal-32k";
    case CartridgeKind::ThreeK3K: return "3k-1-bank";
    case CartridgeKind::ThreeK6K: return "3k-2-bank";
    case CartridgeKind::ThreeK12K: return "3k-4-bank";
    case CartridgeKind::Rally16K: return "rally-16k";
    case CartridgeKind::FourInOne4K: return "4in1-4k";
    case CartridgeKind::Unsupported: return "unsupported";
    }
    return "unknown";
}

void Cartridge::Clear() {
    info = {};
    rom.clear();
}

bool Cartridge::Load(std::vector<std::uint8_t> bytes, std::string name,
                     std::string path) {
    Clear();
    if (bytes.empty()) return false;

    info.path = std::move(path);
    info.name = std::move(name);
    info.size = bytes.size();
    info.crc32 = Crc32(bytes.data(), bytes.size());
    rom = std::move(bytes);

    const std::size_t size = rom.size();
    if (size == 0x0800) {
        info.kind = CartridgeKind::Normal2K;
    } else if (size == 0x1000) {
        info.kind = CartridgeKind::Normal4K;
        // Cartridge-hardware metadata, not timing: the Videopac 31/40 PCB
        // ("4-in-1 Row", "Musician") is a 4KB image whose fixed program ROM is
        // the upper 3KB and whose lower 1KB is a P2-banked data ROM. Size alone
        // cannot distinguish it from a standard 4KB cart, so the known PCB
        // images are identified here by their published CRC (MAME software-list
        // `slot=o2_4in1`). This selects a cartridge *device*, never timing or
        // rendering behaviour. See Docs/Development/NG-4_*.
        switch (info.crc32) {
        case 0x3BFEF56Bu: // VP40 4 in 1 Row (Europe)
        case 0x9B5E9356u: // VP40 4 en 1 Ligne (France)
        case 0xAFB23F89u: // VP31 Musician (Europe)
            info.kind = CartridgeKind::FourInOne4K;
            break;
        default:
            break;
        }
    } else if (size == 0x2000) {
        info.kind = CartridgeKind::Normal8K;
    } else if (size == 0x8000) {
        // MAME's raw 32KB image uses the standard cartridge PCB; only the
        // P1-selected 2KB banks in the first 8KB are reachable.
        info.kind = CartridgeKind::Normal32K;
    } else if (size == 0x0C00) {
        info.kind = CartridgeKind::ThreeK3K;
    } else if (size == 0x1800) {
        info.kind = CartridgeKind::ThreeK6K;
    } else if (size == 0x3000) {
        info.kind = CartridgeKind::ThreeK12K;
    } else if (size == 0x4000) {
        // MAME's raw-image default for a 16KB cartridge is the Rally/VP55-60
        // latch mapper. Software-list images using another PCB carry that
        // information explicitly and are handled by their own mapper.
        info.kind = CartridgeKind::Rally16K;
    } else if (IsPowerOfTwo(size) && size > 0x8000) {
        // Larger power-of-two images need an explicit PCB description. Do
        // not guess: an unknown mapper must fail loudly rather than execute
        // arbitrary bytes.
        info.kind = CartridgeKind::Unsupported;
    } else {
        info.kind = CartridgeKind::Unsupported;
    }
    // Cartridge B pin is a PCB property independent of image size/mapper.
    info.b_pin = HasBPin(info.crc32);
    return true;
}

bool Cartridge::LoadFile(const std::string& path) {
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
    std::vector<std::uint8_t> bytes(static_cast<std::size_t>(length));
    const std::size_t read = std::fread(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
    if (read != bytes.size()) return false;

    std::string name = path;
    const std::size_t slash = name.find_last_of("/\\");
    if (slash != std::string::npos) name = name.substr(slash + 1);
    return Load(std::move(bytes), name, path);
}

} // namespace o2em::ng
