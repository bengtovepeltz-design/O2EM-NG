# Changelog

## 0.22.1-beta "Living Room Beta Update 1"

### Added

- In-game controller port switching.
- Xbox Y now switches physical gamepad routing between the two emulated G7000 / Odyssey² joystick ports.
- Brief on-screen controller routing notifications:
  - `CONTROLLER PORTS SWAPPED`
  - `CONTROLLER PORTS NORMAL`
- Controller routing notification is displayed briefly in the lower-left area of the game display.

### Improved

- Single-player games that use different original joystick ports can now be played with one physical controller.
- Users no longer need two physical controllers simply to accommodate game-dependent joystick-port behavior.
- Original G7000 / Odyssey² joystick-port behavior remains preserved internally while the SDL3 input layer handles physical controller routing.
- Physical controller routing can be changed instantly during gameplay.
- Two-controller multiplayer remains fully supported.

### Testing

- Controller port switching tested successfully in Bowling-Basketball single-player mode.
- Two-controller multiplayer retested successfully in Gunfighter.
- Controller port switching confirmed not to interfere with normal two-player controller operation.
- Xbox Y successfully toggles between normal and swapped controller routing during gameplay.
- On-screen controller routing notification confirmed working during gameplay.

### Notes

The original Philips Videopac G7000 / Magnavox Odyssey² hardware did not have a universal standard for which joystick port a single-player game used.

Some games expect joystick port 1 while others expect joystick port 2.

O2EM-NG continues to emulate this original behavior, but version 0.22.1-beta adds a convenience layer in the SDL3 input system. A single physical controller can now be switched between the two emulated joystick ports by pressing Xbox Y.

This keeps the emulated machine behavior authentic while making the emulator easier to use with modern controllers.

---

## 0.22.0-beta "Living Room Beta"

### Added

- First public Beta release of O2EM-NG.
- Fullscreen startup mode for a more console-like experience.
- SDL3 fullscreen frontend flow.
- In-game Xbox controller shortcuts:
  - Xbox B resets the emulated machine.
  - Xbox Back/View returns from the running game to the ROM browser.
- Confirmed multiplayer controller gameplay.
- Confirmed Gunfighter two-player gameplay with real-world testing.
- Compatibility testing expanded across the current ROM set.
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
- Explicit PAL and NTSC overrides are enforced after original compatibility rules.
- Console reporting for requested region setting and active PAL/NTSC video mode.
- Community testing plan for NTSC behavior.
- Public Windows x64 Beta distribution package.

### Improved

- ROM browser now feels closer to a dedicated living-room console frontend.
- Controller flow improved for couch play.
- Returning from game to frontend is possible without keyboard use.
- Resetting games no longer requires exiting the emulator.
- Fullscreen mode better matches the intended living-room experience.
- Settings can be changed without leaving the frontend.
- Settings persist between emulator launches.
- Source layout cleaned up by removing the obsolete emulator source subfolder after project paths were corrected and verified.
- Release build configuration completed for Windows x64.
- Windows console/debug window removed from the Release build.

### Fixed

- In-game controller shortcut handling works through SDL gamepad events.
- Reset and return-to-browser actions are mapped correctly during emulation.
- Esc and controller B return correctly from the Settings screen to the ROM browser instead of exiting the frontend.
- Fullscreen OFF correctly starts O2EM-NG in a normal window instead of being overridden by old forced-fullscreen startup code.
- Release build SDL3 include, library, and linker configuration corrected.
- BIOS startup verified using the expected `o2rom.bin` BIOS filename.

### Compatibility

Current test status:

- Most tested games are playable with video, sound, keyboard/controller input, and frontend return.
- Gunfighter confirmed working in two-player mode.
- Atlantis confirmed working.
- Munchkin confirmed working.
- Pickaxe Pete confirmed working.
- Bowling-Basketball confirmed working.
- Cosmic Conflict confirmed working.
- Frogger confirmed working.
- Golf confirmed working.
- Spacemonster confirmed working.
- Skiing confirmed working.
- Speedway + Spin-out + Crypto-logic confirmed working.
- Stone Sling confirmed working.
- Air-Sea War & Battle confirmed working.
- Electronic Billiards confirmed working.
- Additional titles have also been tested successfully.
- AUTO, PAL, and NTSC region settings all launch successfully in current testing.
- PAL reports 50 FPS mode.
- NTSC reports 60 FPS mode.
- AUTO preserves the original O2EM compatibility selection behavior.
- External NTSC-region testing remains an important Beta priority.

### Known Issues

- Four in 1 Row currently boots to a grey screen in O2EM-NG.
- The same ROM has been confirmed working in O2EM 1.20B5.
- Investigation has shown:
  - ROM loads correctly.
  - CRC is detected correctly.
  - 4 KB / two-bank cartridge layout is detected.
  - CPU executes cartridge code.
  - External IRQ path is reached.
  - The issue may relate to special EXROM mapping behavior or another emulation-core difference.
- Four in 1 Row investigation is paused after extensive debugging and will be revisited through comparison with known working O2EM implementations.

### Beta Release

- Development source layout cleaned and rebuilt successfully.
- BIOS, ROMS, BOXART, MANUALS, and DOCS distribution folders are in place.
- Data folders contain explanatory README files.
- Copyright and attribution information is maintained in `DOCS/COPYRIGHTS.txt`.
- Custom `.gitignore` prevents BIOS files, ROM files, build output, and local development artifacts from being committed.
- Release x64 build completed.
- Release package tested outside the development folder.
- Public GitHub repository published.
- First public Beta release published.

### Notes

- Four in 1 Row has officially earned the title of first O2EM-NG nemesis ROM.
- O2EM-NG v0.22.0-beta marked the first public release of the project.
- O2EM-NG development continues through testing, compatibility investigation, and community feedback.