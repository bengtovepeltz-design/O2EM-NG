#pragma once
// O2EM-NG Thomson EF9340/EF9341 character generator ROM (2560 bytes).
//
// 256 characters x 10 scanlines, layout used by MAME's ef9340_1 device:
//   0x000..0x3FF  alphanumeric characters 0..127
//   0x400..0x9FF  block/graphics characters 128..255
// Provenance: the Videopac+ character table shipped with O2EM
// (vpp_cset.cpp, O2EM 1.20/1.21) and already present in this repository.
// It differs from MAME's BAD_DUMP datasheet image; see the NG-5 provenance
// notes. Do not edit the values.

#include <cstdint>

namespace o2em::ng {
extern const std::uint8_t kEf9340Charset[2560];
}
