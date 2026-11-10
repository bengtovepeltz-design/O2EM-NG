#include "adapter.h"

namespace o2em::ng {

namespace {

// O2EM key id -> (row, bit) on the i824x keyboard matrix. These ids match
// ../../o2em_keys.h. The ordering is the physical 74156/74148 matrix used by
// the console (and by MAME's KEY.0..KEY.5 ports). It is not a user keymap:
// changing SDL bindings does not change this table.
struct KeyCell {
    std::uint8_t row;
    std::uint8_t bit;
};

// Indices 0..46; index 0 is unused.
constexpr KeyCell kKeyCells[kO2emKeyMax + 1] = {
    {0, 0}, // 0: none
    {4, 0}, // 1  A
    {4, 5}, // 2  B
    {4, 3}, // 3  C
    {3, 2}, // 4  D
    {2, 2}, // 5  E
    {3, 3}, // 6  F
    {3, 4}, // 7  G
    {3, 5}, // 8  H
    {2, 6}, // 9  I
    {3, 6}, // 10 J
    {3, 7}, // 11 K
    {1, 6}, // 12 L
    {4, 6}, // 13 M
    {5, 5}, // 14 N
    {2, 7}, // 15 O
    {1, 7}, // 16 P
    {3, 0}, // 17 Q
    {2, 3}, // 18 R
    {3, 1}, // 19 S
    {2, 4}, // 20 T
    {2, 5}, // 21 U
    {4, 4}, // 22 V
    {2, 1}, // 23 W
    {4, 2}, // 24 X
    {5, 4}, // 25 Y
    {4, 1}, // 26 Z
    {0, 0}, // 27 0
    {0, 1}, // 28 1
    {0, 2}, // 29 2
    {0, 3}, // 30 3
    {0, 4}, // 31 4
    {0, 5}, // 32 5
    {0, 6}, // 33 6
    {0, 7}, // 34 7
    {1, 0}, // 35 8
    {1, 1}, // 36 9
    {1, 4}, // 37 SPACE
    {5, 7}, // 38 ENTER
    {4, 7}, // 39 STOP / .
    {1, 5}, // 40 /
    {2, 0}, // 41 + (keypad)
    {5, 0}, // 42 -
    {5, 1}, // 43 *
    {5, 2}, // 44 / (keypad)
    {5, 3}, // 45 =
    {5, 6}, // 46 CLEAR / DEL
};

} // namespace

bool O2emKeyMatrixPosition(int o2em_key, int& row, int& bit) {
    if (o2em_key <= 0 || o2em_key > kO2emKeyMax) return false;
    const KeyCell& c = kKeyCells[o2em_key];
    // Ids with no matrix position are represented by bit >= 8.
    if (c.bit >= 8 || c.row >= 8) return false;
    row = c.row;
    bit = c.bit;
    return true;
}

bool SetO2emKey(MachineInput& input, int o2em_key, bool pressed) {
    int row = 0, bit = 0;
    if (!O2emKeyMatrixPosition(o2em_key, row, bit)) return false;
    const std::uint8_t mask = static_cast<std::uint8_t>(1u << bit);
    if (pressed)
        input.keyboard.rows[row] |= mask;
    else
        input.keyboard.rows[row] &= static_cast<std::uint8_t>(~mask);
    return true;
}

void SetO2emKeyboard(MachineInput& input, const bool* pressed, int count) {
    for (int i = 0; i < 8; ++i) input.keyboard.rows[i] = 0;
    if (!pressed) return;
    if (count > kO2emKeyMax + 1) count = kO2emKeyMax + 1;
    for (int key = 1; key < count; ++key)
        if (pressed[key]) SetO2emKey(input, key, true);
}

void SetO2emControllers(MachineInput& input, const ControllerState& p1,
                        const ControllerState& p2) {
    input.p1 = p1;
    input.p2 = p2;
}

MachineInput BuildMachineInput(const bool* pressed, int count,
                               const ControllerState& p1,
                               const ControllerState& p2, bool reset) {
    MachineInput input{};
    SetO2emKeyboard(input, pressed, count);
    SetO2emControllers(input, p1, p2);
    input.reset = reset;
    return input;
}

} // namespace o2em::ng
