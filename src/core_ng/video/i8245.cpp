// O2EM-NG i8245 PAL hardware profile.
//
// Geometry is the MAME 0.289 i8245 default: 456 pixels/line, 313 lines,
// VBLANK start 242, HBLANK 366..453, BG sync 414. PAL and NTSC are two
// instances of the same VDC architecture; nothing title-specific lives here.

#include "i8245.h"

namespace o2em::ng {

HardwareProfile I8245PalProfile() {
    return MakePalProfile();
}

} // namespace o2em::ng
