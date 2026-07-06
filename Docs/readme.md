# O2EM-NG

## Philips Videopac G7000 / Magnavox Odyssey² Emulator

O2EM-NG is a modern SDL3-based continuation of the original O2EM emulator.

The goal of the project is to preserve the original O2EM emulation core while modernizing the platform around it with SDL3 video, audio, input, controller support, an integrated frontend, and a simpler living-room experience.

O2EM-NG is currently approaching its first closed Early Access / Beta testing stage.

## Current Features

- SDL3 video, audio, and event system
- Integrated ROM browser and launcher
- Integrated Settings screen
- Persistent configuration through `o2em-ng.cfg`
- Fullscreen or windowed startup
- Region selection: Auto, PAL, and NTSC
- Original O2EM CRC-based compatibility behavior preserved in Auto mode
- PAL 50 FPS and NTSC 60 FPS video modes
- Keyboard support
- Xbox-compatible controller support
- Two simultaneous physical controllers
- Controller navigation in the ROM browser and Settings screen
- In-game controller shortcuts for reset and return to browser
- JPG and PNG box art support
- Original O2EM CPU and VDC emulation core
- Working SDL3 game audio
- Playable commercial Videopac / Odyssey² games
- Two-player gameplay with physical controllers

## Current Status

The project has reached a stable playable pre-beta milestone.

Confirmed playable games include:

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

Additional titles have also been tested successfully.

Two-player gameplay has been successfully tested using two physical controllers simultaneously. Gunfighter multiplayer has been confirmed working with video, sound, and controller input.

The main known compatibility issue is currently Four in 1 Row, which remains under investigation.

Development is now focused on final Beta preparation, BIOS usability, Release x64 testing, documentation, packaging, compatibility testing, and feedback from testers in NTSC regions.

## Settings

The Settings screen is available directly from the ROM browser.

Current settings include:

- Startup display mode: Fullscreen / Windowed
- Region mode: Auto / PAL / NTSC
- Scanlines setting stored in configuration for future renderer integration

Settings are saved in:

`o2em-ng.cfg`

### Region Modes

**Auto** preserves the original O2EM CRC-based compatibility behavior and is the recommended default for Beta testing.

**PAL** explicitly selects the PAL 50 FPS video mode.

**NTSC** explicitly selects the NTSC 60 FPS video mode.

External feedback from testers familiar with real Odyssey² NTSC behavior is especially welcome.

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

## Required Files

O2EM-NG does not include BIOS files, commercial game ROMs, copyrighted box artwork, or commercial game manuals.

Users must provide compatible files themselves.

Local content folders:

```text
BIOS/
ROMS/
BOXART/
MANUALS/
```

Each content folder contains a small README describing what belongs there.

BIOS automatic detection is planned before the first Beta package.

## Beta Testing

Useful compatibility reports should include:

- Game name
- ROM CRC if available
- Country or expected region
- Selected mode: Auto, PAL, or NTSC
- Reported active video mode
- Video behavior
- Speed behavior
- Sound behavior
- Controller used
- Any controller problems

NTSC-region testing is particularly valuable because most current development and testing has been performed in Sweden.

## Documentation

Detailed project documentation is available in the `DOCS` folder:

- `PROJECT.md` – Development status, architecture, milestones, and roadmap
- `CHANGELOG.md` – Version history and changes
- `COPYRIGHTS.txt` – Copyright, licensing, and original O2EM attribution

## Original O2EM Project

O2EM-NG is based on the original O2EM emulator project.

Original O2EM authors:

- Daniel Boris
- Andre de la Rocha
- Arlindo M. de Oliveira

Original copyright notices, attribution, and licensing information are preserved in `DOCS/COPYRIGHTS.txt`.

## O2EM-NG

Modern SDL3 modernization, frontend development, integration, and project development:

**Bengt-Ove Peltz**

Copyright © 2026 Bengt-Ove Peltz

---

**Old hardware. Original games. Modern platform.**
