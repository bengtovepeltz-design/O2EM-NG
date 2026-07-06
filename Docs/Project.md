# O2EM-NG Project Notes

## Project Goal

O2EM-NG is a modernized Philips Videopac G7000 /
Magnavox Odyssey² emulator project based on the original O2EM emulator.

The goal is to preserve the original O2EM emulation core while building
a modern SDL3-based platform around it.

The project aims to provide:

- Integrated ROM browser
- Built-in game launcher
- Modern keyboard and controller support
- Two-player physical controller support
- Fullscreen living-room style operation
- Windowed desktop operation
- Working SDL3 video and audio
- Persistent configuration
- PAL and NTSC region control
- Simple Windows x64 distribution
- Future box art and manual integration
- Future display effects and compatibility improvements
- A clean and maintainable modern platform layer around the original core


# Current Project Status

O2EM-NG has reached its first practical Beta milestone.

The project is no longer only an SDL3 migration experiment or emulator
prototype.

It is now a complete playable application with:

- SDL3 video
- SDL3 audio
- SDL3 window and event handling
- Integrated ROM browser
- Integrated launcher
- Keyboard input
- Xbox-compatible controller support
- Xbox 360 controller support
- Xbox One controller support
- D-pad navigation
- Left analog stick navigation
- Fire button support
- Two simultaneous physical controllers
- Two-player gameplay
- Return from game to frontend
- In-game reset
- Integrated Settings screen
- Persistent `o2em-ng.cfg` configuration
- Fullscreen/windowed startup selection
- Auto/PAL/NTSC region selection
- Working Windows x64 Release build
- Console-free Windows Release executable
- Beta-ready distribution folder structure

The Release x64 build has been successfully built and tested outside the
normal Visual Studio development workflow.

A complete test using BIOS, ROM loading, game launch, video, sound,
controller input, gameplay, and return to frontend has been completed
successfully.


# Current Version

## 0.22.0 "Living Room Beta"

This version represents the transition from internal development
prototype to early Beta testing.

Main identity:

- Fullscreen or windowed startup
- Controller-friendly living-room operation
- Integrated ROM browser
- Integrated launcher
- Integrated Settings screen
- Persistent configuration
- Auto/PAL/NTSC region selection
- SDL3 video and audio
- Keyboard support
- Modern controller support
- Multiplayer support
- Authentic original controller-port behavior
- In-game controller shortcuts
- Clean Windows x64 Release build
- Public source repository
- Known compatibility issue tracking
- Preparation for external testing


# Current User Experience

The emulator now behaves much more like a dedicated console frontend.

Typical use:

1. Start `O2EM-NG.exe`.
2. The ROM browser opens.
3. Choose a game using keyboard or controller.
4. Launch the game.
5. Play using keyboard or compatible controllers.
6. Press Xbox B to reset the running game.
7. Press Xbox Back/View to return to the ROM browser.
8. Open Settings from the ROM browser.
9. Change display mode or region mode.
10. Settings are saved to `o2em-ng.cfg`.
11. Launch another game without restarting the application.

The frontend can be operated from the couch without requiring the
keyboard for normal controller-based use.


# Controls

## ROM Browser

### Keyboard

- Up / Down: Move selection
- Enter: Select or launch
- Esc: Exit frontend

### Controller

- D-pad Up / Down: Move selection
- Left stick Up / Down: Move selection
- A: Select or launch
- B: Exit frontend


## Settings Screen

### Keyboard

- Up / Down: Move selection
- Enter: Change selected setting
- Esc: Return to ROM browser

### Controller

- D-pad Up / Down: Move selection
- Left stick Up / Down: Move selection
- A: Change selected setting
- B: Return to ROM browser


## In Game

### Keyboard

- Esc: Return to ROM browser
- F5: Reset emulated machine

### Controller

- D-pad / left stick: Joystick movement
- A: Fire
- Xbox B: Reset emulated machine
- Xbox Back/View: Return to ROM browser


# Original G7000 Controller-Port Behavior

The Philips Videopac G7000 / Magnavox Odyssey² software library does
not use one universal joystick port for every single-player game.

Different games may read different controller ports.

O2EM-NG currently preserves this behavior rather than forcing every
single-player game onto one standardized modern controller port.

Practical consequences:

- Some single-player games use Controller 1.
- Some single-player games use Controller 2.
- Users may need to change which physical controller they use when
  changing games.
- If a game starts correctly but does not respond to one controller,
  the user should try the other controller.
- Two-player games currently require two connected physical controllers.
- A game responding to the opposite controller should not automatically
  be considered an input emulation bug.

This behavior matches the project developer's memory and experience of
using the original Philips Videopac G7000 hardware.

The current Beta therefore keeps this behavior close to the original
machine.

A future convenience feature may provide optional:

- Swap Controller Ports
- Single Controller Port Selection
- Automatic Single-Player Port Mapping
- Per-game Controller Port Preference

Such features should be optional convenience layers.

The authentic hardware behavior should remain available and should not
be silently removed.


# Confirmed Working Features

## Video

- SDL3 renderer works.
- Game display works.
- Frontend rendering works.
- Fullscreen startup works.
- Windowed startup works.
- Frontend redraw after returning from game works.
- Larger game-window scaling has been improved.
- PAL and NTSC video modes can be selected through Settings.


## Audio

- SDL3 audio works.
- Game sound confirmed working.
- Gunfighter sound confirmed working.
- Munchkin sound confirmed working.
- Multiple games tested successfully with audio.


## Input

- Keyboard input works.
- Xbox 360 controller detected and tested.
- Xbox One controller detected and tested.
- Two controllers can be connected simultaneously.
- Player input works during gameplay.
- D-pad input works.
- Analog stick input works.
- Fire button works.
- In-game controller shortcuts work.
- Gunfighter multiplayer confirmed with two players.
- Original game-dependent controller-port selection behavior is
  preserved.


## Frontend

- ROM browser works.
- Game launching works.
- Returning from game to ROM browser works.
- Settings appears as an entry in the ROM browser.
- Settings screen supports keyboard navigation.
- Settings screen supports controller navigation.
- Esc returns from Settings to the ROM browser.
- Controller B returns from Settings to the ROM browser.
- Settings changes are saved.
- Startup preferences are restored when the emulator starts.
- Empty ROM folder is handled cleanly.


# Settings System

The integrated Settings screen currently provides:

- Start Fullscreen: ON / OFF
- Region Mode: AUTO / PAL / NTSC
- Scanlines: ON / OFF configuration value

Settings are stored in:

`o2em-ng.cfg`

Current implementation:

- Fullscreen/windowed startup setting is active.
- Region selection is active.
- Scanlines setting is stored persistently.
- Settings are available directly from the ROM browser.
- Keyboard navigation is supported.
- Controller navigation is supported.

Future Settings work:

- Connect the scanlines option to the renderer.
- Controller assignment options.
- Per-player controller selection.
- Optional controller-port swapping.
- Possibly sound mute.
- Possibly additional display options.


# Region / PAL / NTSC Status

Region selection is implemented and saved through the Settings system.

Available modes:

- Auto
- PAL
- NTSC

Beta default:

- Auto


## Auto Mode

Auto preserves the original O2EM CRC-based compatibility behavior.

This is the recommended default for Beta testing.


## PAL Mode

PAL explicitly forces PAL timing after the original compatibility rules
have run.

The core reports the PAL 50 FPS video mode.


## NTSC Mode

NTSC explicitly forces NTSC timing after the original compatibility
rules have run.

The core reports the NTSC 60 FPS video mode.


## Current Region Testing Status

AUTO, PAL, and NTSC have all been launch-tested successfully.

Current testing has shown:

- No crashes when switching region modes.
- PAL mode launches games.
- NTSC mode launches games.
- AUTO mode preserves original compatibility behavior.
- Requested region mode is passed from frontend settings into the core.

Development and most current testing has been performed in Sweden.

External testing from users familiar with real Magnavox Odyssey² NTSC
behavior remains an important Beta goal.


# Compatibility Testing

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

Additional tested games are also working.

Most tested games currently provide:

- Video
- Sound
- Keyboard input
- Controller input
- Game launching
- Return to frontend
- Stable gameplay

The major current known exception is Four in 1 Row.


# Gunfighter Multiplayer Milestone

Gunfighter has been tested in real two-player mode using two physical
controllers.

Result:

- Two-player gameplay works.
- Two physical controllers work simultaneously.
- Sound works.
- Gameplay is stable.
- Controller input is responsive.
- Wife-unit approval received.

This was an important project milestone.

It demonstrated that O2EM-NG was no longer simply booting ROM images,
but had become usable for real multiplayer gameplay in a living-room
environment.


# Four in 1 Row Investigation

Four in 1 Row currently does not work correctly in O2EM-NG.

Current behavior:

- ROM loads.
- Screen remains grey.
- Game does not reach the visible SELECT GAME screen.
- Keyboard input does not start the game.


## Known Facts

- The same ROM works in old O2EM through the older frontend.
- Old O2EM displays SELECT GAME.
- Pressing 1 and Enter starts the game in old O2EM.
- The ROM file itself is considered valid.


## Debugging Performed

The following has been confirmed or tested:

- ROM size: 4096 bytes.
- CRC: 3BFEF56B.
- Cartridge layout detected as two-bank 4 KB ROM.
- Bank value detected as 2.
- CPU executes cartridge code.
- T1 timing is not simply stuck.
- External IRQ is taken.
- Interrupt vector points into cartridge code.
- Inverted bank selection tested.
- Alternate initial bank tested.
- Sound-disabled startup tested.
- Keyboard input after grey screen tested.


## Current Theory

Four in 1 Row may require the special EXROM mapping mode mentioned in
historical O2EM documentation.

Historical O2EM notes mention:

- Support added for Four in 1 Row.
- Addition of the `-exrom` switch.
- Special ROM mapping behavior.

This strongly suggests that Four in 1 Row may require cartridge mapping
behavior that O2EM-NG does not currently enable.


## Current Status

Investigation is paused after extensive debugging.

The game remains a known compatibility issue and will be revisited after
the first Beta testing round.

Internal nickname:

Four in 1 Row is the official first O2EM-NG nemesis ROM.


# Windows Release Build

The first Beta-targeted Windows x64 Release build has been completed.


## Release Configuration Work

The Release configuration required separate correction because Visual
Studio maintained different project settings between Debug and Release.

Corrected areas included:

- SDL3 include directories
- SDL3_ttf include directories
- Project include directories
- SDL3 library directory
- SDL3 linker dependency
- SDL3_ttf linker dependency
- Microsoft CRT compatibility setting
- Windows subsystem configuration
- Application entry point configuration


## Console-Free Release Application

The development project uses the standard C/C++:

`main()`

entry point.

For the Release configuration, the application is built using the
Windows subsystem with:

`mainCRTStartup`

as the linker entry point.

This allows O2EM-NG to retain the existing `main()` implementation while
running as a normal Windows graphical application without opening the
development console window.


## Release Runtime Verification

Confirmed with the Release x64 build:

- Executable starts successfully.
- SDL3 runtime loads.
- Frontend window opens.
- No console window appears.
- ROM browser renders correctly.
- Empty ROM folder is handled correctly.
- Controller detection works.
- BIOS loading works.
- ROM loading works.
- Game launching works.
- Video works.
- Sound works.
- Controller input works.
- Gameplay works.
- Return to frontend works.
- Settings load correctly.
- Settings save correctly.

A complete Release build gameplay test was successfully performed before
Beta packaging.


# BIOS Requirements

O2EM-NG does not distribute copyrighted BIOS files.

Users must provide a compatible BIOS image themselves.

The current Beta build expects the BIOS file at:

`BIOS/o2rom.bin`

The BIOS documentation must clearly explain this filename requirement.

No BIOS ROM is included in:

- Source repository
- Release package
- Documentation archive


# Distribution Folder Structure

The Beta package uses the following structure:

O2EM-NG/
- O2EM-NG.exe
- SDL3.dll
- o2em-ng.cfg
- BIOS/
- ROMS/
- BOXART/
- MANUALS/
- DOCS/

Content folders contain explanatory README files where appropriate.

Users must provide their own compatible:

- BIOS image
- Game ROMs
- Box artwork
- Commercial manuals

Copyrighted BIOS files and commercial game ROMs must never be included
in the public repository or Beta package.


# GitHub / Source Distribution

The O2EM-NG source repository has been prepared for public source
distribution.

Completed work:

- Clean repository created.
- Source tree reviewed.
- Initial source commit completed.
- Remote repository configured.
- Source pushed successfully.
- `.gitignore` corrected and verified.
- Visual Studio temporary files excluded.
- Debug output excluded.
- Release output excluded.
- Local BIOS files excluded.
- Local ROM files excluded.
- Local box artwork excluded.
- Temporary and backup files excluded.


## Repository Safety Rules

Never distribute:

- BIOS ROM files
- Commercial game ROM files
- Unauthorized copyrighted artwork
- Unauthorized commercial manuals

Do not commit:

- Visual Studio build output
- Local development databases
- Temporary files
- Debug binaries
- Release binaries
- Local runtime copies of DLL files

Keep visible:

- Original O2EM attribution
- O2EM-NG project history
- Copyright information
- `DOCS/COPYRIGHTS.txt`
- Project documentation
- Source code history


# Source Layout Status

The development tree has been cleaned.

The active source files now live together in the main project source
folder.

The old temporary `emulator/` source subfolder was removed after:

- Project include paths were corrected.
- Source references were corrected.
- Project rebuilt successfully.
- Runtime testing succeeded.

This cleanup reduces confusion and prepares the project for future
source organization.


# Future Source Architecture

A future cleanup may separate the project into clearer architectural
areas.

Possible structure:

src/
- core/
- platform/
- frontend/
- common/

Conceptually:


## core

Original O2EM emulation logic and adapted core systems.

Possible files:

- cpu
- vmachine
- memory
- vdc
- cartridge mapping
- character ROM data


## platform

Modern SDL3 platform integration.

Possible systems:

- SDL3 video
- SDL3 audio
- SDL3 input
- Keyboard input
- Controller management


## frontend

O2EM-NG user interface systems.

Possible systems:

- ROM browser
- Launcher
- Settings screen
- Box art
- Game information
- Manual viewer


## common

Shared utility systems.

Possible systems:

- CRC handling
- File utilities
- Configuration helpers

This restructuring is not required for the first Beta and should not
delay testing.


# Planned Features

## Display

Completed:

- SDL3 video output
- Windowed startup
- Fullscreen startup
- Persistent display-mode setting

Planned:

- Aspect-ratio preservation improvements
- Functional scanline effect
- Optional old-TV appearance
- Future shader/filter support


## Audio

Current status:

- SDL3 audio works.
- Game sound works.

Future possibilities:

- Mute option
- Internal volume control if needed

System volume control is currently considered sufficient for the first
Beta.


## Controller

Current status:

- Xbox-compatible controller support
- Xbox 360 controller tested
- Xbox One controller tested
- D-pad support
- Analog stick support
- Fire button support
- Two simultaneous controllers
- Multiplayer gameplay
- In-game reset shortcut
- Return-to-browser shortcut
- Original game-dependent controller-port behavior preserved

Planned improvements:

- Better player assignment options
- Controller reconnect handling
- Controller status display
- Optional controller mapping screen
- Per-player controller selection
- Optional controller-port swapping
- Possible automatic single-player port selection


## Frontend

Completed:

- ROM browser
- Game launcher
- Controller navigation
- Settings entry
- Settings screen
- Keyboard navigation
- Controller navigation
- Persistent configuration

Planned improvements:

- Box art display
- Game details panel
- Favorites
- Last played
- Compatibility status display
- Cleaner Beta branding
- Manual viewer


## Manuals

Future goal:

Allow users to provide their own manual files and access them from the
frontend.

Possible future support:

- PDF manual launch
- Image-based manual pages
- Controller-friendly page navigation
- Per-game manual association

No commercial manuals will be distributed with O2EM-NG.


# Current Development Philosophy

The project follows a practical development philosophy:

1. Make the emulator pleasant to use.
2. Preserve currently working compatibility.
3. Test real games.
4. Test real controllers.
5. Release an early Beta.
6. Collect tester feedback.
7. Fix compatibility problems based on real reports.
8. Improve architecture carefully without breaking working games.

Accuracy improvements remain important.

However, difficult compatibility bugs should not prevent practical
testing of the large number of games that already work.

The Four in 1 Row investigation demonstrated that a single compatibility
problem can consume many hours of development time.

The current strategy is therefore:

- Preserve the working compatibility baseline.
- Release the Beta.
- Collect real-world reports.
- Prioritize problems based on tester experience.
- Return to difficult edge cases with better evidence.


# Authenticity and Convenience Philosophy

O2EM-NG should preserve original hardware and software behavior wherever
practical.

At the same time, future optional convenience features may improve the
experience for modern users.

The preferred design philosophy is:

- Preserve authentic behavior by default.
- Clearly document original hardware behavior.
- Avoid treating authentic behavior as an emulator defect.
- Add convenience features as optional layers.
- Do not silently change game behavior simply to make the emulator feel
  more like a modern console.

The controller-port behavior is an example of this philosophy.

The current Beta preserves the game-dependent controller-port behavior.

A future version may add automatic or manual port swapping as an
optional convenience feature.


# Regression Testing Rule

Before major changes to:

- CPU timing
- VDC timing
- Interrupt behavior
- Cartridge banking
- ROM mapping
- PAL/NTSC behavior
- Input timing
- Controller-port routing

the known working game collection should be retested.

The current compatibility baseline must be protected while the project
continues to modernize.


# Beta Testing Goals

The first Beta testing round should focus on:

- General game compatibility
- NTSC behavior
- PAL behavior
- AUTO region behavior
- Controller compatibility
- Original controller-port behavior
- Two-player controller behavior
- Sound behavior
- Video speed
- Return-to-browser stability
- Settings persistence
- Fullscreen startup
- Windowed startup
- Clean startup on systems without the development environment


# Beta Compatibility Report Template

Useful tester reports should include:

- O2EM-NG version
- Windows version
- Game name
- ROM CRC if available
- Country or expected region
- Selected region mode: Auto / PAL / NTSC
- Expected hardware region
- Video behavior
- Game speed behavior
- Sound behavior
- Controller model
- Number of connected controllers
- Which physical controller worked
- Which physical controller did not work
- Whether both controller ports were tested
- Whether the game is single-player or multiplayer
- Any controller behavior problems
- Whether return to frontend worked
- Whether reset worked
- Any crash or error message

NTSC-region feedback is particularly valuable because current development
and most local testing has been performed in Sweden.


# Completed Beta Preparation

Completed:

- SDL3 migration
- SDL3 video
- SDL3 audio
- ROM browser
- Game launcher
- Keyboard input
- Controller input
- Analog stick support
- D-pad support
- Two-player controller support
- In-game reset
- Return to ROM browser
- Settings screen
- Persistent configuration
- Fullscreen/windowed setting
- Auto/PAL/NTSC region setting
- Compatibility testing
- Original controller-port behavior documentation
- Known issue documentation
- Copyright documentation
- Clean source layout
- Git repository preparation
- `.gitignore` verification
- Initial source publication
- Distribution folders
- Folder README files
- Windows Release x64 build
- Release SDL3 configuration
- Console-free Windows executable
- Release runtime folder assembly
- Release BIOS loading test
- Release ROM loading test
- Release gameplay test
- Clean startup test


# Remaining Beta Release Tasks

1. Final review of `CHANGELOG.md`.
2. Final review of `PROJECT.md`.
3. Final review of public `README.md`.
4. Remove all local test BIOS and ROM files from the Release package.
5. Verify the clean package contents.
6. Create the Windows x64 Beta ZIP.
7. Create the GitHub Release.
8. Attach the Beta ZIP.
9. Mark the release as a pre-release.
10. Publish short tester instructions.
11. Begin external NTSC-region testing.
12. Collect compatibility reports.
13. Freeze non-critical feature work during the first testing round.


# Current Assessment

O2EM-NG has reached the point where external Beta testing is justified.

The emulator currently provides:

- A working original O2EM-derived emulation core
- Modern SDL3 video
- Modern SDL3 audio
- Integrated ROM browser
- Integrated launcher
- Keyboard input
- Modern gamepad input
- Two-player controller gameplay
- Original game-dependent controller-port behavior
- In-game reset
- Controller-based return to frontend
- Integrated Settings screen
- Persistent configuration
- PAL/NTSC/AUTO region selection
- Fullscreen and windowed operation
- Clean Windows x64 Release build
- Console-free graphical startup
- Public source repository
- Compatibility documentation
- Known issue tracking

The immediate focus is no longer getting the project to compile or
proving that games can run.

The immediate focus is now:

- Packaging
- Beta publication
- External testing
- NTSC-region feedback
- Compatibility reports
- Controller behavior reports
- Regression prevention
- Careful fixes based on real tester experience

O2EM-NG 0.22.0 "Living Room Beta" represents the first point where the
project can be handed to other users as a complete playable emulator
application for meaningful testing.


---

Old hardware. Original games. Modern platform.

O2EM-NG

Modern SDL3 modernization, frontend development, integration, and
project development:

Bengt-Ove Peltz

Copyright © 2026 Bengt-Ove Peltz

Based on the original O2EM emulator by:

Daniel Boris  
Andre de la Rocha  
Arlindo M. de Oliveira