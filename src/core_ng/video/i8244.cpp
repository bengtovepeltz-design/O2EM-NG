// O2EM-NG i8244 NTSC hardware profile.
//
// Geometry is the MAME 0.289 i8244 default: 455 pixels/line, 263 lines,
// VBLANK start 242, HBLANK 366..452, BG sync 413. The NTSC console pixel clock
// is the 7.15909 MHz crystal; the CPU divides it by 4/3 (see timebase.h).

#include "i8244.h"

namespace o2em::ng {

HardwareProfile I8244NtscProfile() {
    return MakeNtscProfile();
}

} // namespace o2em::ng
