// O2EM-NG NG host input integration conformance (controller regression).
//
// This test exercises the REAL existing input chain:
//   SDL virtual gamepad -> InputSDL/InputManager -> JoyState
//   -> NG adapter -> MachineInput -> MachineBus BUS joystick pins.
// It deliberately does not reimplement mapping tables.

#include <SDL3/SDL.h>

#include <cstdint>
#include <cstdio>

#include "input_manager.h"
#include "input_sdl.h"
#include "src/ng_host/host_commands.h"
#include "o2em_keys.h"
#include "src/core_ng/input/adapter.h"
#include "src/core_ng/machine_config.h"
#include "src/core_ng/memory/bus.h"
#include "src/core_ng/timebase.h"

// The input layer's non-North-button message hook is not used by these tests.
void VDCStub_ShowMessage(const char*) {}

namespace {

int g_checks = 0;
int g_failures = 0;

void Check(bool condition, const char* what) {
    ++g_checks;
    if (!condition) {
        ++g_failures;
        std::printf("FAIL  %s\n", what);
    }
}

void CheckEq(std::uint64_t actual, std::uint64_t expected, const char* what) {
    ++g_checks;
    if (actual != expected) {
        ++g_failures;
        std::printf("FAIL  %s (got %llu, expected %llu)\n", what,
                    static_cast<unsigned long long>(actual),
                    static_cast<unsigned long long>(expected));
    }
}

o2em::ng::ControllerState ToNg(const JoyState& j) {
    o2em::ng::ControllerState c;
    c.up = j.up; c.down = j.down; c.left = j.left;
    c.right = j.right; c.fire = j.fire;
    return c;
}

o2em::ng::MachineInput BuildFromManager() {
    const JoyState j1 = InputManager_GetPlayer1State();
    const JoyState j2 = InputManager_GetPlayer2State();
    const o2em::ng::ControllerState c1 = ToNg(j1);
    const o2em::ng::ControllerState c2 = ToNg(j2);
    return o2em::ng::BuildMachineInput(nullptr, 0, c1, c2, false);
}

std::uint8_t ReadBus(const o2em::ng::MachineInput& input, int player) {
    static o2em::ng::MachineBus bus;
    static bool configured = false;
    static o2em::ng::HardwareProfile profile = o2em::ng::MakePalProfile();
    if (!configured) {
        bus.Configure(profile, nullptr, nullptr, nullptr, nullptr, nullptr,
                      const_cast<o2em::ng::MachineInput*>(&input), nullptr);
        configured = true;
    } else {
        bus.Configure(profile, nullptr, nullptr, nullptr, nullptr, nullptr,
                      const_cast<o2em::ng::MachineInput*>(&input), nullptr);
    }
    bus.PortWrite(1, 0x00); // P12 low: joystick matrix enabled
    bus.PortWrite(2, static_cast<std::uint8_t>(player));
    return bus.BusRead();
}

void PushButton(SDL_JoystickID id, SDL_GamepadButton button, bool down) {
    SDL_Event e{};
    e.type = down ? SDL_EVENT_GAMEPAD_BUTTON_DOWN : SDL_EVENT_GAMEPAD_BUTTON_UP;
    e.gbutton.which = id;
    e.gbutton.button = button;
    e.gbutton.down = down;
    InputManager_HandleEvent(e);
}

void PushAxis(SDL_JoystickID id, SDL_GamepadAxis axis, Sint16 value) {
    SDL_Event e{};
    e.type = SDL_EVENT_GAMEPAD_AXIS_MOTION;
    e.gaxis.which = id;
    e.gaxis.axis = axis;
    e.gaxis.value = value;
    InputManager_HandleEvent(e);
}

SDL_JoystickID AttachPad(const char* name) {
    SDL_VirtualJoystickDesc desc{};
    SDL_INIT_INTERFACE(&desc);
    desc.type = SDL_JOYSTICK_TYPE_GAMEPAD;
    desc.naxes = SDL_GAMEPAD_AXIS_COUNT;
    desc.nbuttons = SDL_GAMEPAD_BUTTON_COUNT;
    desc.name = name;
    desc.vendor_id = 0x1234;
    desc.product_id = 0x5678;
    return SDL_AttachVirtualJoystick(&desc);
}

void TestChain(SDL_JoystickID id1, SDL_JoystickID id2) {
    // Neutral.
    InputManager_ResetSessionState();
    InputSDL_EnsureGamepadsOpen();
    InputSDL_RefreshGamepadStates();
    {
        const auto mi = BuildFromManager();
        Check(!mi.p1.up && !mi.p1.down && !mi.p1.left && !mi.p1.right &&
                  !mi.p1.fire,
              "chain: neutral");
        CheckEq(ReadBus(mi, 0), 0xFF, "chain: neutral BUS value");
    }

    struct Case {
        SDL_GamepadButton button;
        bool (o2em::ng::ControllerState::*field);
        std::uint8_t bus;
        const char* name;
    };
    const Case cases[] = {
        {SDL_GAMEPAD_BUTTON_DPAD_UP, &o2em::ng::ControllerState::up, 0xFE,
         "chain: up"},
        {SDL_GAMEPAD_BUTTON_DPAD_DOWN, &o2em::ng::ControllerState::down, 0xFB,
         "chain: down"},
        {SDL_GAMEPAD_BUTTON_DPAD_LEFT, &o2em::ng::ControllerState::left, 0xF7,
         "chain: left"},
        {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, &o2em::ng::ControllerState::right, 0xFD,
         "chain: right"},
        {SDL_GAMEPAD_BUTTON_SOUTH, &o2em::ng::ControllerState::fire, 0xEF,
         "chain: fire"},
    };
    for (const Case& c : cases) {
        InputManager_ResetSessionState();
        InputSDL_EnsureGamepadsOpen();
        PushButton(id1, c.button, true);
        const auto mi = BuildFromManager();
        Check(mi.p1.*(c.field), c.name);
        CheckEq(ReadBus(mi, 0), c.bus, c.name);
        PushButton(id1, c.button, false);
        Check(!(BuildFromManager().p1.*(c.field)), "chain: released returns neutral");
    }

    // Diagonal, from the real SDL event path.
    InputManager_ResetSessionState();
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    {
        const auto mi = BuildFromManager();
        Check(mi.p1.up && mi.p1.right, "chain: diagonal up+right");
        CheckEq(ReadBus(mi, 0), 0xFC, "chain: diagonal BUS value");
    }
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);

    // Left stick through the refresh path (virtual pad, player 2 slot).
    SDL_Joystick* j2 = SDL_GetJoystickFromID(id2);
    Check(j2 != nullptr, "virtual pad handle present");
    if (j2) {
        InputManager_ResetSessionState();
        SDL_SetJoystickVirtualAxis(j2, SDL_GAMEPAD_AXIS_LEFTX, 30000);
        SDL_UpdateJoysticks();
        SDL_PumpEvents();
        InputSDL_RefreshGamepadStates();
        const auto mi = BuildFromManager();
        Check(mi.p2.right && !mi.p2.left, "chain: left stick right via refresh");
        CheckEq(ReadBus(mi, 1), 0xFD, "chain: stick right BUS value");
        SDL_SetJoystickVirtualAxis(j2, SDL_GAMEPAD_AXIS_LEFTX, 0);
        SDL_UpdateJoysticks();
        SDL_PumpEvents();
        InputSDL_RefreshGamepadStates();
        Check(!BuildFromManager().p2.right, "chain: stick neutral refresh");
    }

    // Player 2 routing.
    InputManager_ResetSessionState();
    PushButton(id2, SDL_GAMEPAD_BUTTON_DPAD_LEFT, true);
    {
        const auto mi = BuildFromManager();
        Check(mi.p2.left && !mi.p1.left, "chain: player 2 left");
        CheckEq(ReadBus(mi, 1), 0xF7, "chain: player 2 BUS value");
    }
    PushButton(id2, SDL_GAMEPAD_BUTTON_DPAD_LEFT, false);

    // Player 1 and player 2 simultaneously.
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_UP, true);
    PushButton(id2, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
    {
        const auto mi = BuildFromManager();
        Check(mi.p1.up && mi.p2.down, "chain: both players independent");
        CheckEq(ReadBus(mi, 0), 0xFE, "chain: p1 BUS with both held");
        CheckEq(ReadBus(mi, 1), 0xFB, "chain: p2 BUS with both held");
    }
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_UP, false);
    PushButton(id2, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);

    // Keyboard + joystick coexistence.
    SDL_Event key{};
    key.type = SDL_EVENT_KEY_DOWN;
    key.key.key = SDLK_UP;
    key.key.scancode = SDL_GetScancodeFromKey(SDLK_UP, nullptr);
    InputManager_HandleEvent(key);
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, true);
    {
        const auto mi = BuildFromManager();
        Check(mi.p1.up && mi.p1.right, "chain: keyboard + joystick coexist");
        CheckEq(ReadBus(mi, 0), 0xFC, "chain: keyboard+joystick BUS");
    }
    PushButton(id1, SDL_GAMEPAD_BUTTON_DPAD_RIGHT, false);
    SDL_Event keyup = key;
    keyup.type = SDL_EVENT_KEY_UP;
    InputManager_HandleEvent(keyup);

    // Repeated per-frame refresh must be stable and must survive session reset.
    if (j2) {
        InputManager_ResetSessionState();
        SDL_SetJoystickVirtualButton(j2, SDL_GAMEPAD_BUTTON_DPAD_DOWN, true);
        for (int frame = 0; frame < 10; ++frame) {
            SDL_UpdateJoysticks();
            SDL_PumpEvents();
            InputSDL_RefreshGamepadStates();
            const auto mi = BuildFromManager();
            Check(mi.p2.down, "chain: state refresh across frames");
        }
        InputManager_ResetSessionState();
        Check(!BuildFromManager().p2.down, "chain: session reset neutral");
        SDL_UpdateJoysticks();
        SDL_PumpEvents();
        InputSDL_RefreshGamepadStates();
        Check(BuildFromManager().p2.down, "chain: relaunch reconnects state");
        SDL_SetJoystickVirtualButton(j2, SDL_GAMEPAD_BUTTON_DPAD_DOWN, false);
        SDL_UpdateJoysticks();
        SDL_PumpEvents();
        InputSDL_RefreshGamepadStates();
    }
}

void TestEnterKeyboardMatrix() {
    bool pressed[o2em::ng::kO2emKeyMax + 1] = {};
    pressed[KEY_ENTER] = true;
    const o2em::ng::MachineInput input =
        o2em::ng::BuildMachineInput(pressed, o2em::ng::kO2emKeyMax + 1,
                                   o2em::ng::ControllerState{},
                                   o2em::ng::ControllerState{}, false);
    CheckEq(input.keyboard.rows[5], 0x80,
            "C7010 input: ENTER is keyboard row 5 bit 7");
    bool otherRowsClear = true;
    for (int row = 0; row < 8; ++row) {
        if (row != 5 && input.keyboard.rows[row] != 0)
            otherRowsClear = false;
    }
    Check(otherRowsClear, "C7010 input: ENTER does not leak into other rows");

    // The bus presents the same active-high row through the 74148 encoder.
    o2em::ng::MachineBus bus;
    o2em::ng::HardwareProfile profile = o2em::ng::MakePalProfile();
    o2em::ng::MachineInput mutableInput = input;
    bus.Configure(profile, nullptr, nullptr, nullptr, nullptr, nullptr,
                  &mutableInput, nullptr);
    bus.PortWrite(1, 0x00); // P12 low: keyboard decoder enabled
    bus.PortWrite(2, 0x05); // row 5
    CheckEq(bus.PortRead(2), 0x0F,
            "C7010 input: row 5 ENTER reaches the 8048 P2 encoder");
}

void TestHostCommands() {
    using o2em::ng::host::HostCommandState;
    SDL_Event e{};

    HostCommandState f5;
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_F5;
    f5.HandleEvent(e);
    Check(f5.TakeReset(), "host: F5 requests reset");
    Check(!f5.TakeReset(), "host: reset request is edge-triggered");
    Check(!f5.QuitRequested(), "host: F5 does not exit");

    HostCommandState esc;
    e = SDL_Event{};
    e.type = SDL_EVENT_KEY_DOWN;
    e.key.key = SDLK_ESCAPE;
    esc.HandleEvent(e);
    Check(esc.QuitRequested(), "host: ESC returns to frontend");

    HostCommandState east;
    e = SDL_Event{};
    e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    e.gbutton.button = SDL_GAMEPAD_BUTTON_EAST;
    east.HandleEvent(e);
    Check(east.TakeReset(), "host: gamepad East requests reset");
    Check(!east.QuitRequested(), "host: gamepad East does not exit");

    HostCommandState back;
    e = SDL_Event{};
    e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    e.gbutton.button = SDL_GAMEPAD_BUTTON_BACK;
    back.HandleEvent(e);
    Check(back.QuitRequested(), "host: gamepad Back returns to frontend");
    Check(!back.TakeReset(), "host: gamepad Back does not reset");

    HostCommandState fire;
    e = SDL_Event{};
    e.type = SDL_EVENT_GAMEPAD_BUTTON_DOWN;
    e.gbutton.button = SDL_GAMEPAD_BUTTON_SOUTH;
    fire.HandleEvent(e);
    Check(!fire.QuitRequested() && !fire.TakeReset(),
          "host: fire stays an input, not a command");

    HostCommandState close;
    e = SDL_Event{};
    e.type = SDL_EVENT_QUIT;
    close.HandleEvent(e);
    Check(close.QuitRequested(), "host: window close exits");
}

} // namespace

int main() {
    std::printf("NG host input integration conformance\n");
#if defined(_WIN32)
    // Keep synthetic SDL events deterministic; the XInput backend is covered
    // by the real-app runtime trace.
    _putenv_s("O2EM_INPUT_DISABLE_XINPUT", "1");
#endif
    TestEnterKeyboardMatrix();
    TestHostCommands();
    if (!SDL_Init(SDL_INIT_GAMEPAD | SDL_INIT_JOYSTICK)) {
        std::printf("SKIP: SDL gamepad subsystem unavailable: %s\n",
                    SDL_GetError());
        return 0;
    }
    AttachPad("NG Test Pad 1");
    AttachPad("NG Test Pad 2");
    SDL_PumpEvents();

    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    Check(count >= 2, "at least two SDL gamepads available for test");
    SDL_JoystickID id1 = 0, id2 = 0;
    if (ids && count >= 1) id1 = ids[0];
    if (ids && count >= 2) id2 = ids[1];
    SDL_free(ids);
    InputSDL_EnsureGamepadsOpen();
    if (id1 && id2) TestChain(id1, id2);
    SDL_Quit();
    std::printf("\nchecks: %d, failures: %d\n", g_checks, g_failures);
    if (g_failures == 0) {
        std::printf("RESULT: GREEN - NG host input integration passed.\n");
        return 0;
    }
    std::printf("RESULT: RED - NG host input integration failures.\n");
    return 1;
}
