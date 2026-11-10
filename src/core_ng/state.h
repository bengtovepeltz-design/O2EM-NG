#pragma once
// NG-0 snapshot contract. Every mutable device participates.

#include <cstddef>
#include <cstdint>
#include <vector>

namespace o2em::ng {

using Snapshot = std::vector<std::uint8_t>;

struct SnapshotWriter {
    Snapshot bytes;
    void u8(std::uint8_t v) { bytes.push_back(v); }
    void u16(std::uint16_t v) { u8(v & 0xff); u8(v >> 8); }
    void u32(std::uint32_t v) { u16(v & 0xffff); u16(v >> 16); }
    void u64(std::uint64_t v) { u32(v & 0xffffffffull); u32(v >> 32); }
    void raw(const void* p, std::size_t n) {
        const auto* b = static_cast<const std::uint8_t*>(p);
        bytes.insert(bytes.end(), b, b + n);
    }
};

struct SnapshotReader {
    const std::uint8_t* p = nullptr;
    std::size_t n = 0;
    std::size_t at = 0;
    bool ok = true;

    std::uint8_t u8() { if (at + 1 > n) { ok = false; return 0; } return p[at++]; }
    std::uint16_t u16() { auto a = u8(); auto b = u8(); return a | (b << 8); }
    std::uint32_t u32() { auto a = u16(); auto b = u16(); return a | (b << 16); }
    std::uint64_t u64() { auto a = u32(); auto b = u32(); return a | (static_cast<std::uint64_t>(b) << 32); }
    void raw(void* dst, std::size_t count) {
        if (at + count > n) { ok = false; return; }
        std::uint8_t* d = static_cast<std::uint8_t*>(dst);
        for (std::size_t i = 0; i < count; ++i) d[i] = p[at++];
    }
};

} // namespace o2em::ng
