#pragma once
// O2EM-NG hardware-level input adapter.
//
// This is the only bridge between the existing O2EM-NG logical controls and
// the new machine's electrical input model. It deliberately does NOT define
// user bindings: the frontend still maps SDL keys/joysticks to the existing
// O2EM key ids (o2em_keys.h) and JoyState. This adapter maps those logical
// actions onto the i824x keyboard matrix and joystick port pins exactly as the
// hardware (and MAME's KEY.x / joystick ports) does.
//
// The matrix wiring below is hardware semantics, not a user keymap:
//   row0: 0 1 2 3 4 5 6 7
//   row1: 8 9 - - SPACE / L P
//   row2: + W E R T U I O
//   row3: Q S D F G H J K
//   row4: A Z X C V B M STOP
//   row5: - * / = Y N CLEAR ENTER
// Rows 6/7 are unused. Bit 7 is the priority-encoder's highest-priority key.

#include <cstdint>

#include "input.h"

namespace o2em::ng {

// Valid O2EM logical key ids are 1..46 (see o2em_keys.h). 0 means "none".
constexpr int kO2emKeyMax = 46;

// Map one O2EM key id to (row, bit). Returns false for unused/out-of-range ids.
bool O2emKeyMatrixPosition(int o2em_key, int& row, int& bit);

// Set/clear one key in the hardware matrix.
bool SetO2emKey(MachineInput& input, int o2em_key, bool pressed);

// Build the full matrix from the existing per-key pressed array (indexed by
// O2EM key id). `count` may be kO2emKeyMax + 1.
void SetO2emKeyboard(MachineInput& input, const bool* pressed, int count);

// Copy the existing joystick state into the hardware controller pins.
void SetO2emControllers(MachineInput& input, const ControllerState& p1,
                        const ControllerState& p2);

// Convenience: build a complete MachineInput from the app-level state.
MachineInput BuildMachineInput(const bool* pressed, int count,
                               const ControllerState& p1,
                               const ControllerState& p2,
                               bool reset = false);

} // namespace o2em::ng
