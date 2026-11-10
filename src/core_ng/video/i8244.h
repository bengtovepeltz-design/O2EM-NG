#pragma once
// NG-0 i8244 NTSC profile.

#include "../timebase.h"
#include "vdc.h"

namespace o2em::ng {

// i8244 NTSC, as used by Odyssey 2 / Videopac G7000 NTSC:
// 455 pixels/line, 263 lines/frame, VBL line 242 h=365,
// HBlank 366..452, BG sync 413.
HardwareProfile I8244NtscProfile();

} // namespace o2em::ng
