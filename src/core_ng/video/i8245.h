#pragma once
// NG-0 i8245 PAL profile.

#include "../timebase.h"
#include "vdc.h"

namespace o2em::ng {

// i8245 PAL, as used by Videopac G7000 PAL and G7400:
// 456 pixels/line, 313 lines/frame, VBL line 242 h=365,
// HBlank 366..453, BG sync 414.
HardwareProfile I8245PalProfile();

} // namespace o2em::ng
