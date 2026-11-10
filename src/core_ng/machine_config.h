#pragma once
// O2EM-NG machine configuration and public information.
//
// Region is defined by the authoritative timebase. PAL and NTSC are profile
// selections, not separate code paths.

#include <cstdint>
#include <string>

#include "timebase.h"

namespace o2em::ng {

struct MachineConfig {
    std::string bios_path;
    std::string cartridge_path;
    Region region = Region::Auto;
    bool scanlines = false;
    bool vpp_enabled = false; // G7400 / Videopac+ EF934x + composition
    bool g7400 = false;       // select the G7400 machine profile

    // Deterministic test-rig profile. Production code uses Region. Tests use
    // this to exercise IRQ/T1 scheduling without waiting a full console frame.
    bool use_custom_profile = false;
    HardwareProfile custom_profile{};
};

struct MachineInfo {
    std::string bios_name;
    std::string cartridge_name;
    Region active_region = Region::Auto;
    std::uint32_t cartridge_crc = 0;
    std::size_t cartridge_size = 0;
    std::string cartridge_kind;
    int video_width = 0;
    int video_height = 0;
    double frame_rate = 0.0;
    std::int64_t frame_ticks = 0;
    std::int64_t cpu_hz_numerator = 0;
    std::int64_t cpu_hz_denominator = 1;
};

} // namespace o2em::ng
