# NG host input regression — Xbox controller

Date: 2026-11-10
Scope: NG development host only. No frontend, mapping, adapter or machine
semantics changed.

## Symptom

The Xbox controller worked with the legacy execution path but not under
`O2EM_CORE=ng`.

## First point of loss

The physical controller was never given an `InputSDL` handle in the NG host.
The existing input layer opens gamepads from `SDL_EVENT_GAMEPAD_ADDED`. The NG
host starts after the frontend has already consumed/aged that event, so
`gamepads[0]` remained null. `InputManager_GetPlayer1State()` therefore stayed
neutral no matter what the controller did. The adapter and machine BUS were
correct; they were receiving neutral state.

`O2EM_INPUT_TRACE=1` confirmed the sequence at NG start:

```text
INPUT_JOY p1 byte=FF ...          (neutral from session reset)
Gamepad 1 connected: Xbox One Controller
INPUT_GAMEPAD slot=1 id=7 name=Xbox One Controller
```

## Fix

Minimal existing-input-layer integration helpers in `input_sdl.cpp/.h`:

- `InputSDL_EnsureGamepadsOpen()` enumerates `SDL_GetGamepads()` and opens any
  already-connected controller through the existing `OpenGamepad()`. It skips
  controllers already open in a slot.
- `InputSDL_RefreshGamepadStates()` reads the open gamepads' current D-pad,
  South button and left-stick axes once per frame and feeds the existing
  `PushStates()` path. This is a lossless fallback for missed SDL events.

The NG host calls `InputSDL_EnsureGamepadsOpen()` before the first frame and
`InputSDL_RefreshGamepadStates()` after each event-pump, before
`BuildInput()`. No mapping table, button assignment, deadzone, port-swap
behaviour or user setting changed. The legacy path still uses the event path
and is unaffected.

## Tests

`tests/input_ng/input_ng_conformance.cpp` exercises the real chain:

```text
SDL gamepad/keyboard -> input_sdl/input_manager -> JoyState
-> NG adapter -> MachineInput -> MachineBus BUS pins
```

With a real Xbox controller present and SDL virtual pads, it covers neutral,
up/down/left/right/fire, diagonal, stick axis, player 1, player 2, keyboard +
joystick coexistence, per-frame refresh, reset and relaunch.

Result: 43 checks, 0 failures (Debug and Release).

The NG Debug application was also launched with `O2EM_INPUT_TRACE=1`; it opened
the Xbox One Controller and ran `vp_48.bin` under the NG core.

## Round 2 — real app vs test harness

The first fix opened the controller and refreshed SDL gamepad state, and the
synthetic SDL integration suite passed. The real Debug app still failed because
the physical Xbox One device is an **XInput** controller and SDL3's cached
gamepad polling state was not reliable for it in the real host loop. The test
harness only pushed synthetic SDL button/axis events; it never exercised the
actual XInput device state used by the app.

Real app trace (`O2EM_INPUT_TRACE=1`):

- `Xbox One Controller`: SDL slot 1, type 3, vendor `045E`, product `02DD`.
- Virtual Xbox 360 test pad: SDL slot 2, type 2, vendor `045E`, product `028E`.
- Holding an XInput pad before the app opened it produced
  `INPUT_REFRESH ... state=00000` from SDL, i.e. cached SDL state was neutral.
- Physical-controller test then reached the emulated BUS:

```text
INPUT_XINPUT slot=1 user=0 buttons=0001 ...
INPUT_JOY p1 byte=FE up=1 ...
INPUT_MACHINE p1=01 p2=00
INPUT_BUS p1 byte=FE

INPUT_XINPUT slot=1 user=0 buttons=1000 ...   (A / fire)
INPUT_JOY p1 byte=EF fire=1
INPUT_BUS p1 byte=EF
```

Physical D-pad up/left/right and A/fire were observed through InputManager,
MachineInput and the emulated BUS. Left-stick directions were observed as
XInput `state=00010/00100/01000` axis states.

### Second fix

`input_sdl.cpp` now uses XInput as the backend for **Xbox-class SDL gamepads**
(`SDL_GAMEPAD_TYPE_XBOX360` / `XBOXONE`), mapping XInput state onto the same
`JoyState` slots. Non-Xbox controllers continue through SDL. XInput user 0 maps
to the first Xbox slot, user 1 to the next; the user index is released and
reassigned on disconnect/reconnect. Keyboard state remains combined as before.

The backend is generic by device class, not by title or button name. User
mappings, deadzone semantics and slot assignment are unchanged. The physical
test above is the acceptance evidence; the synthetic integration suite remains
deterministic via `O2EM_INPUT_DISABLE_XINPUT=1`.
