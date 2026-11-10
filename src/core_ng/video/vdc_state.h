#pragma once
// O2EM-NG real i8244/i8245 VDC state contract.
//
// The register file is the documented 256-byte i824x space. The byte offsets
// below are the hardware offsets used by the MAME i8244 implementation and by
// the Philips documentation:
//
//   0x00..0x0F  4 minor sprites   (y, x, color, unused)
//   0x10..0x3F  12 major objects  (y, x, ptr, color)
//   0x40..0x7F  4 quads x 4 chars (y, x, ptr, color)
//   0x80..0x9F  4 sprites x 8 shape bytes
//   0xA0 control, 0xA1 status, 0xA2 collision, 0xA3 color
//   0xA4 Y beam, 0xA5 X beam, 0xA6 unused
//   0xA7..0xA9 sound shift registers, 0xAA sound control
//   0xC0..0xC8 horizontal grid low, 0xD0..0xD8 high, 0xE0..0xE9 vertical
//
// Registers 0xA0..0xAF are mirrored at 0xB0..0xBF. Quad x/y registers are
// mirrored to the first character of each quad. Registers >= 0xF0 are
// unmapped. The full mask/mirror behavior lives in Vdc::FixRegisterMirrors.

#include <cstdint>

namespace o2em::ng {

constexpr int kVdcRegisterCount = 0x100;
constexpr int kVdcVisibleWidth = 360;
constexpr int kVdcVisibleHeight = 243;
constexpr int kVdcCollisionMapSize = 0x200;
constexpr int kVdcCharacterRomSize = 0x200;

// Register offsets used by the renderer.
constexpr std::uint8_t kRegMinor = 0x00;
constexpr std::uint8_t kRegMajor = 0x10;
constexpr std::uint8_t kRegQuad = 0x40;
constexpr std::uint8_t kRegShape = 0x80;
constexpr std::uint8_t kRegControl = 0xA0;
constexpr std::uint8_t kRegStatus = 0xA1;
constexpr std::uint8_t kRegCollision = 0xA2;
constexpr std::uint8_t kRegColor = 0xA3;
constexpr std::uint8_t kRegYBeam = 0xA4;
constexpr std::uint8_t kRegXBeam = 0xA5;
constexpr std::uint8_t kRegShift1 = 0xA7;
constexpr std::uint8_t kRegShift2 = 0xA8;
constexpr std::uint8_t kRegShift3 = 0xA9;
constexpr std::uint8_t kRegSound = 0xAA;
constexpr std::uint8_t kRegHGrid0 = 0xC0;
constexpr std::uint8_t kRegHGrid1 = 0xD0;
constexpr std::uint8_t kRegVGrid = 0xE0;

// A0 control bits.
constexpr std::uint8_t kControlStrobe = 0x02;
constexpr std::uint8_t kControlSoundIrq = 0x04;
constexpr std::uint8_t kControlGrid = 0x08;
constexpr std::uint8_t kControlExternalCollisionIrq = 0x10;
constexpr std::uint8_t kControlDisplay = 0x20;
constexpr std::uint8_t kControlGridDots = 0x40;
constexpr std::uint8_t kControlGridWide = 0x80;

// A1 status bits (latched, cleared by reading A1 except the live bits).
constexpr std::uint8_t kStatusSoundIrq = 0x04;
constexpr std::uint8_t kStatusVblank = 0x08;
constexpr std::uint8_t kStatusExternalCollision = 0x40;
constexpr std::uint8_t kStatusMajorCollision = 0x80;

// A2 collision source bits.
constexpr std::uint8_t kCollisionMinor0 = 0x01;
constexpr std::uint8_t kCollisionMinor1 = 0x02;
constexpr std::uint8_t kCollisionMinor2 = 0x04;
constexpr std::uint8_t kCollisionMinor3 = 0x08;
constexpr std::uint8_t kCollisionGridVertical = 0x10;
constexpr std::uint8_t kCollisionGridHorizontal = 0x20;
constexpr std::uint8_t kCollisionExternal = 0x40;
constexpr std::uint8_t kCollisionMajor = 0x80;

// Logical hardware palette indices 0..15. Background uses 0..7 (low
// intensity), objects/sprites use 8..15 (high intensity). The canonical RGB
// values are MAME's pre-RF i8244 palette; presentation palette work is out of
// NG-3 scope.
struct VdcRgb {
    std::uint8_t r;
    std::uint8_t g;
    std::uint8_t b;
};
extern const VdcRgb kI8244PaletteRgb[16];

} // namespace o2em::ng
