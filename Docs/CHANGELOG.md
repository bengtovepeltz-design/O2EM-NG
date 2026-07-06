# Changelog

## 0.22.0 "Living Room Beta"

First Beta release milestone for O2EM-NG.

This release marks the transition from development prototype to a
playable, controller-friendly SDL3-based emulator package suitable
for early external testing.


### Added

- Fullscreen and windowed startup modes.
- SDL3 fullscreen frontend flow.
- Integrated ROM browser and launcher.
- Integrated Settings entry in the ROM browser.
- Controller- and keyboard-navigable Settings screen.
- Persistent `o2em-ng.cfg` configuration system.
- Saved startup display setting for fullscreen/windowed mode.
- Region mode selection:
  - Auto
  - PAL
  - NTSC
- Region-mode wiring from frontend settings into the emulator core.
- AUTO mode preserves original O2EM CRC-based compatibility behavior.
- Explicit PAL and NTSC overrides are enforced after original
  compatibility rules.
- In-game Xbox controller shortcuts:
  - Xbox B resets the emulated machine.
  - Xbox Back/View returns from the running game to the ROM browser.
- Two simultaneous physical controller support.
- Confirmed multiplayer controller gameplay.
- Confirmed Gunfighter two-player gameplay with real-world testing.
- Community testing plan for NTSC behavior.
- Beta-ready distribution folder structure:
  - `BIOS/`
  - `ROMS/`
  - `BOXART/`
  - `MANUALS/`
  - `DOCS/`
- README files for local content folders.
- Project-specific `.gitignore` for safe public source distribution.
- Initial public source repository preparation.


### Improved

- ROM browser now feels closer to a dedicated living-room console
  frontend.
- Controller flow improved for couch play.
- Returning from a running game to the frontend is possible without
  keyboard use.
- Resetting games no longer requires exiting the emulator.
- Settings can be changed without leaving the frontend.
- Settings persist between emulator launches.
- Fullscreen/windowed startup preference is restored on launch.
- Compatibility testing expanded across the current ROM collection.
- Source layout cleaned by removing the obsolete temporary emulator
  source subfolder after project paths were corrected and verified.
- Visual Studio Debug and Release configurations were reviewed and
  corrected for SDL3 include and library paths.
- Release x64 SDL3 linking configuration was corrected.
- Release build startup no longer displays the development console
  window.
- Distribution package tested independently from the development
  source tree.
- BIOS folder and filename requirements clarified.
- Controller documentation expanded to explain original G7000
  joystick-port behavior.


### Controller Behavior

O2EM-NG currently preserves the original Philips Videopac G7000 /
Magnavox Odyssey² software behavior where there is no universal
single-player joystick-port convention across all games.

Practical behavior in the current Beta:

- Some single-player games use Controller 1.
- Some single-player games use Controller 2.
- Users may need to use the other physical controller when changing
  games.
- Two-player games currently require two connected physical
  controllers.
- A game responding to the opposite controller is not automatically
  considered an input emulation failure.

This behavior is intentionally preserved for the first Beta rather
than forcing all games onto a standardized modern controller port.

A future optional convenience feature may provide controller-port
swapping or automatic single-controller mapping without changing the
authentic default behavior.


### Fixed

- In-game controller shortcut handling now works through SDL gamepad
  events.
- Reset and return-to-browser actions are mapped correctly during
  emulation.
- Esc and controller B return correctly from the Settings screen to
  the ROM browser instead of exiting the frontend.
- Fullscreen OFF correctly starts O2EM-NG in a normal window instead
  of being overridden by old forced-fullscreen startup code.
- Release configuration SDL3 include paths corrected.
- Release configuration SDL3 library path corrected.
- SDL3 and SDL3_ttf linker dependencies aligned with the working
  development configuration.
- Microsoft CRT compatibility warnings handled for the Release build.
- Windows Release subsystem and entry point configuration corrected,
  eliminating the unwanted console window.
- BIOS startup testing confirmed with the expected BIOS filename:
  `BIOS/o2rom.bin`.


### Release Build Verification

The Windows x64 Release build has been successfully created and
runtime-tested.

Confirmed:

- Release x64 builds successfully.
- `O2EM-NG.exe` starts outside the Visual Studio development workflow.
- SDL3 runtime loads correctly.
- No development console window appears during normal startup.
- ROM browser starts correctly.
- Empty ROM folder is handled cleanly.
- BIOS loading works with `BIOS/o2rom.bin`.
- ROM discovery works.
- Game launching works.
- Video output works.
- Sound output works.
- Keyboard input works.
- Xbox-compatible controller input works.
- Games can return to the ROM browser.
- Settings load and save correctly.
- Fullscreen and windowed startup modes work.
- A complete BIOS + ROM gameplay test was successfully performed using
  the Release build.


### Compatibility

Current test status:

- Most tested games are playable with video, sound,
  keyboard/controller input, and frontend return.
- Gunfighter confirmed working in two-player mode.
- Two physical controllers can be used simultaneously.
- Atlantis confirmed playable.
- Munchkin confirmed playable.
- Pickaxe Pete confirmed playable.
- Bowling-Basketball confirmed playable.
- Cosmic Conflict confirmed playable.
- Frogger confirmed playable.
- Golf confirmed playable.
- Spacemonster confirmed playable.
- Skiing confirmed playable.
- Speedway + Spin-out + Crypto-logic confirmed playable.
- Stone Sling confirmed playable.
- Air-Sea War & Battle confirmed playable.
- Electronic Billiards confirmed playable.
- Additional games have also passed manual gameplay testing.
- AUTO, PAL, and NTSC region settings all launch successfully in
  current testing.
- PAL reports the core's 50 FPS video mode.
- NTSC reports the core's 60 FPS video mode.
- AUTO preserves the original O2EM compatibility selection behavior.
- External NTSC-region testing remains a Beta priority.


### Known Issues

#### Four in 1 Row

Four in 1 Row currently boots to a grey screen in O2EM-NG.

The same ROM works in the original O2EM environment.

Investigation confirmed:

- ROM loads correctly.
- CRC is detected correctly.
- ROM size is 4096 bytes.
- 4 KB / two-bank cartridge layout is detected.
- CPU executes cartridge code.
- T1 timing is not simply stuck.
- External IRQ path is reached.
- Interrupt vector points into cartridge code.
- Alternate bank behavior has been tested.
- Sound-disabled testing does not resolve the issue.
- Keyboard input after the grey screen does not start the game.

The current theory is that Four in 1 Row may require the special
EXROM mapping mode mentioned in historical O2EM documentation.

The investigation is paused after extensive debugging and will be
revisited after the first Beta testing round.

Four in 1 Row remains the official first O2EM-NG nemesis ROM.


### Beta Preparation

Completed:

- Development source layout cleaned.
- Project rebuilt successfully after source cleanup.
- Public Git repository created.
- Initial clean source commit completed.
- Repository exclusion rules tested.
- BIOS and commercial ROM files excluded from source distribution.
- Build output excluded from source distribution.
- Visual Studio temporary files excluded from source distribution.
- `BIOS`, `ROMS`, `BOXART`, `MANUALS`, and `DOCS` distribution folders
  are in place.
- Data folders contain explanatory README files.
- Copyright and attribution information is maintained in
  `DOCS/COPYRIGHTS.txt`.
- Windows Release x64 build completed successfully.
- Release runtime folder assembled.
- Console-free Release executable confirmed working.
- Release build tested with BIOS and game ROM.
- Release gameplay test completed successfully.
- Clean startup with empty ROM folder confirmed.
- BIOS filename requirement confirmed as `o2rom.bin`.
- Original controller-port behavior documented for Beta testers.

Remaining Beta work:

- Final documentation review.
- Final Beta ZIP assembly.
- GitHub Release creation.
- Short tester guide and compatibility report template.
- External NTSC-region testing.
- Feedback collection from early testers.


### Notes

O2EM-NG 0.22.0 represents the first practical Beta milestone of the
project.

The emulator has progressed from SDL3 migration and early frontend
experiments into a complete playable application with:

- Original O2EM emulation core.
- SDL3 video.
- SDL3 audio.
- Integrated ROM browser.
- Game launcher.
- Keyboard input.
- Modern controller input.
- Two-player controller support.
- Settings system.
- Persistent configuration.
- Auto/PAL/NTSC region selection.
- Fullscreen and windowed operation.
- Clean Windows x64 Release build.
- Controller-friendly living-room operation.

The next phase is focused on real-world Beta testing, compatibility
reports, NTSC-region feedback, controller behavior reports, and careful
fixes based on tester experience.

Old hardware. Original games. Modern platform.