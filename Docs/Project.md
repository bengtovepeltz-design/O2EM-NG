# O2EM-NG Project Notes

## Project Goal

O2EM-NG is a modernized Philips Videopac G7000 / Magnavox Odyssey2 emulator project based on O2EM.

The goal is to create a clean SDL3-based emulator with:

- Integrated ROM browser
- Modern controller support
- Fullscreen living-room style experience
- Working sound
- Keyboard and gamepad input
- Simple beta-ready distribution
- Future settings, scanlines, box art, and compatibility improvements

## Current Status

The project has reached an early playable beta-style state.

Working systems so far:

- SDL3 video
- SDL3 window handling
- Fullscreen or windowed startup from saved settings
- ROM browser
- Game launcher
- Keyboard input
- Xbox controller input
- Left analog stick support
- D-pad support
- Fire button support
- Sound output
- Return from game to frontend
- Reset during gameplay
- Two-player controller gameplay
- Integrated Settings screen
- Persistent `o2em-ng.cfg` configuration
- Region selection: Auto / PAL / NTSC
- Requested and active video-mode console reporting

## Current User Experience

The emulator now behaves much more like a dedicated console frontend:

1. Start O2EM-NG.
2. Fullscreen ROM browser opens.
3. Choose a game using keyboard or controller.
4. Launch game.
5. Play using keyboard or Xbox controllers.
6. Press Xbox Back/View to return to the ROM browser.
7. Press Xbox B to reset the running game.
8. Open Settings from the ROM browser to change startup display mode, region mode, and scanlines setting.
9. Settings are saved to `o2em-ng.cfg`.

This is a major step toward a closed beta.

## Controls

### Frontend

Keyboard:

- Up / Down: select ROM
- Enter: launch game
- Esc: exit frontend

Controller:

- D-pad Up / Down: select ROM
- Left stick Up / Down: select ROM
- A: launch game
- B: exit frontend

### In Game

Keyboard:

- Esc: return to ROM browser
- F5: reset emulated machine

Controller:

- Xbox B: reset emulated machine
- Xbox Back/View: return to ROM browser
- D-pad / left stick: joystick movement
- A: fire

## Confirmed Working Features

### Video

- SDL3 renderer works.
- Game display works.
- Fullscreen startup works.
- Frontend redraw after returning from game works.
- Larger game window scaling has been improved.

### Audio

- SDL3 audio works.
- Game sound confirmed working.
- Gunfighter sound confirmed working.
- Munchkin sound confirmed working.
- Multiple games tested with audio.

### Input

- Keyboard input works.
- Xbox 360 controller detected.
- Xbox One controller detected.
- Two controllers can be connected at once.
- Player input works in games.
- Gunfighter multiplayer confirmed working with two players.

### Frontend

- ROM browser works.
- Game launching works.
- Returning from game to ROM browser works.
- Fullscreen startup gives a better living-room console feel.
- Settings appears as an entry in the ROM browser.
- Settings screen supports keyboard and controller navigation.
- Esc / controller B returns from Settings to the ROM browser.
- Settings changes are saved to `o2em-ng.cfg`.
- Fullscreen/windowed startup preference is restored on launch.

## Compatibility Testing

The current ROM collection has been tested manually.

Confirmed working games include:

- Gunfighter
- Atlantis
- Bowling-Basketball
- Munchkin
- Pickaxe Pete
- Skiing
- Spacemonster
- Speedway + Spin-out + Crypto-logic
- Cosmic Conflict
- Stone Sling
- Air-Sea War & Battle
- Electronic Billiards
- Frogger
- Golf

Additional tested games are also working, with the major current exception being Four in 1 Row.

## Gunfighter Multiplayer Milestone

Gunfighter has been tested in real two-player mode using controllers.

Result:

- Two-player gameplay works.
- Controllers work.
- Sound works.
- Gameplay is stable.
- Wife-unit approval received.

This is an important milestone because it proves the emulator is not only booting games, but actually usable and fun in a real living-room setting.

## Four in 1 Row Investigation

Four in 1 Row currently does not work correctly in O2EM-NG.

Current behavior:

- ROM loads.
- Screen remains grey.
- Game does not reach the visible SELECT GAME screen.
- Keyboard input does not start the game.

Known facts:

- The same ROM works in old O2EM through the older frontend.
- Old O2EM shows SELECT GAME.
- Pressing 1 and Enter starts the game in old O2EM.
- The ROM file itself is considered valid.

Debugging performed:

- Confirmed ROM size: 4096 bytes.
- Confirmed CRC: 3BFEF56B.
- Confirmed cartridge layout: two-bank 4 KB ROM.
- Confirmed bank value: 2.
- Confirmed CPU executes cartridge code.
- Confirmed T1 timing is not simply stuck.
- Confirmed external IRQ is taken.
- Confirmed interrupt vector points into cartridge code.
- Tested inverted bank selection.
- Tested alternate initial bank.
- Tested with sound disabled.
- Tested keyboard input after grey screen.

Current theory:

Four in 1 Row may require the special EXROM mapping mode mentioned in the old O2EM changelog.

Relevant historical clue:

O2EM v1.00 notes mention:

- Added support for the Four in 1 Row game.
- Added the `-exrom` switch for a special ROM mapping mode.

This strongly suggests Four in 1 Row may need special cartridge mapping behavior that O2EM-NG does not currently enable.

Status:

Investigation paused after extensive debugging. The game remains a known compatibility issue and will be revisited later.

Internal nickname:

Four in 1 Row is currently the official O2EM-NG nemesis ROM.

## Region / PAL / NTSC Status

Region selection is now implemented and saved through the Settings system.

Available modes:

- Auto
- PAL
- NTSC

Beta default:

- Auto

Current behavior:

- Auto preserves the original O2EM CRC-based compatibility behavior.
- PAL explicitly forces PAL timing after the original compatibility rules have run.
- NTSC explicitly forces NTSC timing after the original compatibility rules have run.
- PAL uses the core's 50 FPS video mode.
- NTSC uses the core's 60 FPS video mode.
- The console reports both the requested region setting and active video mode.
- AUTO, PAL, and NTSC have all been launch-tested successfully with no crashes or obvious regressions.

Development and most current testing is being done in Sweden, so PAL behavior is easier to verify locally. NTSC feedback from testers in NTSC regions is an important closed-beta goal.

Beta tester feedback should include:

- Game name
- ROM CRC if available
- Country / expected region
- Selected mode: Auto, PAL, or NTSC
- Reported active video mode
- Video behavior
- Speed behavior
- Sound behavior
- Controller used
- Any controller behavior problems

## Closed Beta Goals

Closed beta preparation status:

Completed:

- Clean development source layout
- ROMS folder
- BIOS folder
- BOXART folder
- MANUALS folder
- DOCS folder
- README files in data folders
- Integrated Settings screen
- Persistent configuration
- Fullscreen/windowed startup setting
- Region Auto/PAL/NTSC setting
- Region debug reporting
- Compatibility notes
- Known issue tracking
- Controller instructions
- Legal/copyright notes in `DOCS/COPYRIGHTS.txt`
- Git repository preparation and custom `.gitignore`

Remaining before first beta package:

- BIOS automatic detection
- Release x64 build
- Test the Release build on a machine without the development environment
- Final README.md review
- Basic packaging instructions
- Beta tester guide / feedback template
- NTSC-region community testing

## Current Folder Notes

Important folders:

- BIOS/
- ROMS/
- BOXART/
- MANUALS/
- DOCS/

Each folder should contain a README explaining what belongs there.

Users must provide their own compatible files.

## Planned Features

### Display

Completed:

- Optional windowed/fullscreen startup mode
- Fullscreen setting saved between launches

Planned:

- Aspect-ratio preservation
- Scanline effect
- Optional old-TV look
- Future shader/filter support

### Audio

- Keep current working sound path.
- Internal volume control is not urgent because users can control system volume.
- Optional mute may be added later.

### Controller

Planned improvements:

- Better player assignment options
- Controller reconnect handling
- Controller status display
- Optional controller mapping screen
- Per-player controller selection

### Frontend

Completed:

- ROM browser
- Controller navigation
- Settings entry in ROM browser
- Settings screen
- Settings keyboard/controller navigation
- Persistent configuration file

Planned improvements:

- Box art display
- Game details panel
- Favorites
- Last played
- Compatibility status
- Cleaner beta branding

### Settings

Current implementation:

- Display mode: fullscreen/windowed
- Region mode: Auto/PAL/NTSC
- Scanlines: on/off value stored in configuration
- Settings saved in `o2em-ng.cfg`
- Settings screen accessible from the ROM browser
- Keyboard and controller navigation supported

Next settings work:

- Wire scanlines into the renderer
- Controller assignment
- Possibly sound mute

## Current Development Philosophy

Focus on practical playable progress first.

Accuracy improvements are important, but the current priority is:

1. Make the emulator pleasant to use.
2. Confirm broad compatibility.
3. Prepare a closed beta.
4. Collect tester feedback.
5. Fix compatibility issues based on real reports.

The Four in 1 Row investigation showed that some compatibility bugs can consume many hours. For now, the better path is to keep building the beta and return to difficult edge cases later.

## Current Version Direction

The current working version is moving toward:

## 0.22.0 "Living Room Beta"

Main identity:

- Fullscreen or windowed startup
- Controller-friendly operation
- Integrated Settings screen
- Persistent configuration
- Auto/PAL/NTSC region selection
- Sound working
- Multiplayer working
- Early beta preparation
- Known compatibility issue tracking

## Notes for Future Development

Important future investigation:

- Revisit Four in 1 Row.
- Compare original O2EM EXROM implementation.
- Investigate how `-exrom` affects ROM mapping.
- Add automatic special mapping for CRC 3BFEF56B if needed.
- Avoid breaking the currently working 24/25-style compatibility baseline.

Important reminder:

Before major timing, CPU, VDC, or banking changes, retest the known working games to avoid regressions.

## GitHub / Source Distribution Preparation

The project is being prepared for source publication and closed beta distribution.

Repository safety rules:

- Do not distribute BIOS ROM files.
- Do not distribute commercial game ROM files.
- Do not commit local build output or Visual Studio temporary files.
- Keep project history, original O2EM credits, and copyright information visible.
- `DOCS/COPYRIGHTS.txt` contains the project copyright and attribution information.
- A project-specific `.gitignore` is used to exclude BIOS files, ROM files, build output, and local development artifacts.

The development tree has been cleaned so the active source files now live together in the main project source folder. The old temporary `emulator/` source subfolder has been removed after project paths were corrected and the project was rebuilt and runtime-tested successfully.

## Immediate Pre-Beta Tasks

1. Implement BIOS automatic detection in the `BIOS/` folder.
2. Review and update public-facing README documentation.
3. Build Release x64.
4. Test the Release build outside the development environment.
5. Prepare the beta package.
6. Prepare a short tester guide and compatibility report template.
7. Recruit NTSC-region testers familiar with Odyssey² behavior.
8. Freeze new feature work during the first testing round except for critical fixes.

Current assessment:

O2EM-NG is very close to a closed Early Access / Beta build. The core emulator, SDL3 video and audio, ROM browser, controller support, multiplayer, Settings system, persistent configuration, and region selection are all operational. The remaining work is primarily release preparation, BIOS usability, documentation, packaging, and external compatibility testing.
