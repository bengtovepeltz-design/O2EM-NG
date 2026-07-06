# Changelog

## 0.22.0 "Living Room Beta"

### Added

- Fullscreen startup mode for a more console-like experience.
- SDL3 fullscreen frontend flow.
- In-game Xbox controller shortcuts:
  - Xbox B resets the emulated machine.
  - Xbox Back/View returns from the running game to the ROM browser.
- Confirmed multiplayer controller gameplay.
- Confirmed Gunfighter two-player gameplay with real-world testing.
- Compatibility testing expanded across the current ROM set.
- Initial closed beta planning notes.
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

### Improved

- ROM browser now feels closer to a dedicated living-room console frontend.
- Controller flow improved for couch play.
- Returning from game to frontend is now possible without keyboard use.
- Resetting games no longer requires exiting the emulator.
- Fullscreen mode better matches the intended early beta experience.
- Settings can be changed without leaving the frontend.
- Settings persist between emulator launches.
- Source layout cleaned up by removing the obsolete emulator source subfolder after project paths were corrected and verified.

### Fixed

- In-game controller shortcut handling now works through SDL gamepad events.
- Reset and return-to-browser actions are now mapped correctly during emulation.
- Esc and controller B now return correctly from the Settings screen to the ROM browser instead of exiting the frontend.
- Fullscreen OFF now correctly starts O2EM-NG in a normal window instead of being overridden by old forced-fullscreen startup code.

### Compatibility

Current test status:

- Most tested games are playable with video, sound, keyboard/controller input, and frontend return.
- Gunfighter confirmed working in two-player mode.
- Atlantis, Munchkin, Pickaxe Pete, Bowling-Basketball, Cosmic Conflict, Frogger, Golf, Spacemonster, Skiing, Speedway + Spin-out + Crypto-logic, and others confirmed working during manual testing.
- AUTO, PAL, and NTSC region settings all launch successfully in current testing.
- PAL reports 50 FPS mode and NTSC reports 60 FPS mode.
- AUTO preserves the original O2EM compatibility selection behavior.
- External NTSC-region testing remains a beta priority.

### Known Issues

- Four in 1 Row currently boots to a grey screen in O2EM-NG.
- The same ROM works in old O2EM through the previous frontend setup.
- Investigation showed:
  - ROM loads correctly.
  - CRC is detected correctly.
  - 4 KB / two-bank cartridge layout is detected.
  - CPU executes cartridge code.
  - External IRQ path is reached.
  - The issue may relate to the special EXROM mapping mode mentioned in old O2EM documentation.
- Four in 1 Row investigation is paused after extensive debugging and will be revisited later.

### Beta Preparation

- Development source layout cleaned and rebuilt successfully.
- BIOS, ROMS, BOXART, MANUALS, and DOCS distribution folders are in place.
- Data folders contain explanatory README files.
- Copyright and attribution information is maintained in `DOCS/COPYRIGHTS.txt`.
- Custom `.gitignore` prepared to prevent BIOS files, ROM files, build output, and local development artifacts from being committed.
- Remaining pre-beta work is focused on BIOS automatic detection, Release x64 testing, documentation review, packaging, and external NTSC-region testing.

### Notes

- Four in 1 Row has officially earned the title of first O2EM-NG nemesis ROM.
- Current practical beta status is very promising despite this one compatibility problem.
- O2EM-NG is now considered very close to a closed Early Access / Beta build.