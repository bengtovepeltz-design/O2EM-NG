# O2EM-NG

## Philips Videopac G7000 / Magnavox Odyssey² Emulator

O2EM-NG is a modern SDL3-based continuation of the original O2EM
emulator.

The project preserves the original O2EM emulation core while
modernizing the platform around it with SDL3 video, audio, input,
controller support, an integrated ROM browser, persistent settings,
and a controller-friendly frontend.

O2EM-NG is currently in active development and has reached its first
practical Beta milestone.


## Current Features

- SDL3 video and event system
- SDL3 audio
- Integrated ROM browser
- Integrated game launcher
- Keyboard support
- Xbox-compatible controller support
- Two simultaneous physical controllers
- Two-player gameplay
- D-pad support
- Analog stick support
- In-game reset
- Controller shortcut for returning to the ROM browser
- Integrated Settings screen
- Persistent configuration
- Fullscreen or windowed startup
- Auto / PAL / NTSC region selection
- JPG and PNG box art file support
- Original O2EM CPU and VDC emulation core
- Playable commercial Videopac / Odyssey² games
- Windows x64 Release build


## Current Status

O2EM-NG has reached its first practical Beta milestone.

The emulator has progressed from SDL3 migration and early frontend
development into a complete playable application.

The Windows x64 Release build has been successfully tested with:

- BIOS loading
- ROM discovery
- Game launching
- Video output
- Sound output
- Keyboard input
- Controller input
- Gameplay
- Return to ROM browser
- Settings loading and saving
- Fullscreen startup
- Windowed startup

Most tested games are playable.

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

Additional games have also passed manual gameplay testing.

Two-player gameplay has been successfully tested using two physical
controllers simultaneously.

The main known compatibility issue is currently Four in 1 Row, which
boots to a grey screen and remains under investigation.


## Settings

The Settings screen is available directly from the ROM browser.

Current settings include:

- Startup display mode: Fullscreen / Windowed
- Region mode: Auto / PAL / NTSC
- Scanlines setting stored in configuration for future renderer
  integration

Settings are saved in:

`o2em-ng.cfg`


### Region Modes

**Auto**

Preserves the original O2EM CRC-based compatibility behavior.

Auto is the recommended default for Beta testing.


**PAL**

Explicitly selects the PAL 50 FPS video mode.


**NTSC**

Explicitly selects the NTSC 60 FPS video mode.

External feedback from testers familiar with real Magnavox Odyssey²
NTSC behavior is especially welcome.


## Controls

### ROM Browser and Settings

Keyboard:

- Up / Down: Move selection
- Enter: Select or launch
- Esc: Return from Settings or exit the frontend

Controller:

- D-pad Up / Down: Move selection
- Left stick Up / Down: Move selection
- A: Select or launch
- B: Return from Settings or exit the frontend


### In Game

Keyboard:

- Esc: Return to ROM browser
- F5: Reset the emulated machine

Controller:

- D-pad / left stick: Joystick movement
- A: Fire
- Xbox B: Reset the emulated machine
- Xbox Back/View: Return to ROM browser


### Important Controller Behavior

O2EM-NG currently follows the original Philips Videopac G7000 /
Magnavox Odyssey² controller behavior closely.

The original system does not have a universal convention where every
single-player game always uses the same joystick port.

Depending on the game, a single-player game may use either Controller 1
or Controller 2.

Because of this, you may need to use the other controller when changing
games. If a game starts normally but does not respond to one controller,
try the other controller.

For the current Beta version:

- Two-player games require two connected physical controllers.
- Single-player games may use either Controller 1 or Controller 2,
  depending on the game.
- If a game starts but does not respond to one controller, try the
  other controller.
- Different games may require changing which physical controller is
  used for single-player gameplay.

This behavior reflects the way original G7000 software selected
joystick inputs.

O2EM-NG currently preserves this behavior rather than forcing every
single-player game onto one standardized controller port.

This also matches the project developer's experience with the original
Philips Videopac G7000 hardware.


## Required Files

O2EM-NG does not include BIOS files, commercial game ROMs, copyrighted
box artwork, or commercial game manuals.

Users must provide compatible files themselves.

Local content folders:

```text
BIOS/
ROMS/
BOXART/
MANUALS/