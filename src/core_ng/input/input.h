#pragma once
// O2EM-NG machine-level input.
//
// No SDL keycodes here. The frontend translates its events into these
// controller/keyboard states, and deterministic scheduled transitions let
// tests (and future runtime differential) drive input at exact machine ticks.

#include <cstdint>

#include "../timebase.h"

namespace o2em::ng {

struct ControllerState {
    bool up = false;
    bool down = false;
    bool left = false;
    bool right = false;
    bool fire = false;
};

inline std::uint8_t ControllerBits(const ControllerState& c) {
    // Matches the 824x/joystick matrix wiring used by the console:
    // bit0 up, bit1 right, bit2 down, bit3 left, bit4 fire.
    return static_cast<std::uint8_t>(
        (c.up ? 0x01 : 0) | (c.right ? 0x02 : 0) | (c.down ? 0x04 : 0) |
        (c.left ? 0x08 : 0) | (c.fire ? 0x10 : 0));
}

struct KeyboardMatrix {
    // Eight rows, active-high key bits. Bit 7 is the leftmost key in MAME's
    // priority-encoder order (see MachineBus::PortRead).
    std::uint8_t rows[8] = {};
};

struct MachineInput {
    ControllerState p1;
    ControllerState p2;
    KeyboardMatrix keyboard;
    bool reset = false;
};

enum class InputTarget : std::uint8_t {
    Controller1 = 0,
    Controller2,
    KeyboardRow,
    ResetButton
};

struct InputTransition {
    Tick tick = 0;
    InputTarget target = InputTarget::Controller1;
    std::uint8_t index = 0; // keyboard row, ignored otherwise
    std::uint8_t value = 0; // controller bits, row bits, or reset level
};

} // namespace o2em::ng
