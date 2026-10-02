# O2EM-NG Project Notes

This file is organised so the present state is easy to separate from history:

- **CURRENT STATE** - the present working tree (2 October 2026).
- **CURRENT KNOWN ISSUES**.
- **CURRENT DEVELOPMENT**.
- Historical checkpoints and earlier development, kept for provenance.
- Archived earlier release information.

The current working tree is authoritative; older sections below use "current"
as it applied to the version described there.

## CURRENT STATE - 2 October 2026

| Item | Value |
|------|-------|
| Version | v0.31.0-beta |
| Release name | Beta 4 |
| Release state | Published (current release) |
| Previous published version | v0.30.0-beta (Beta 3) |
| Platform | Windows x64, C/C++, SDL3, Visual Studio 2026 |
| Author | Bengt-Ove Peltz |

The most recent completed work in the working tree is:

- **Win95 window chrome** (`frontend_chrome.h/.cpp`): the SDL window is
  borderless and the frontend draws a custom client-side title bar (application
  icon, white title text, minimize / maximize / close) and a gray menu bar
  (File, Library, Settings, Tools, Help - a visual shell whose entries are not
  yet wired to actions). Dragging the title bar moves the window, double-click
  toggles maximize/restore, the buttons minimize / maximize / close, and the
  edges resize the window. Fullscreen still works and there is no duplicate
  native title bar. Layout constants: title bar 26 px, menu bar 22 px (48 px
  chrome), banner margin 6 px, banner height 110 px, `TabsTop = 170`,
  `ContentTop = 224`. The banner artwork is unchanged and now framed.
- **My Collection** (`src/collection/collection_database.*`,
  `src/frontend/frontend_collection.*`): a dedicated physical-collection page and
  database `GAMEDATA/mycollection.db`, independent of `o2em-ng.db` (the main
  database is read-only). Add / Edit / Remove, Mark / Unmark Wanted, search,
  sorting, quantity and duplicates, statistics, UTF-8 CSV export, separate
  cartridge / box / manual conditions, a UTF-8 multiline Notes editor with
  clipboard support, and a compact Library summary panel with a Wanted (W)
  column.
- **UTF-8 SDL media paths** (`frontend_boxart.cpp`,
  `src/frontend/frontend_screenshot.cpp`): paths are converted to UTF-8 before
  SDL image calls, fixing file names containing non-ASCII characters. This is a
  narrow SDL-facing fix and not a whole-architecture UTF-8 migration.
- **Earlier Beta 4 work**: self-updater Phase 1 (central
  `src/version.h`) and Phase 2 (CHECK UPDATES / VIEW RELEASE) with reliability
  fixes; Library Folders and Collection Statistics; the integrated Cover / Media
  viewer; the full-height scrollable Game Library; DELETE GAME DATA with
  `suppressed_catalog_entries`; N / N+ catalogue identity separation; and the
  VP31 / VP40 XROM work. See `CHANGELOG.md` for the itemised list.

Navigation: Library, Edit Game Data, Import Center, Manual, Settings, About,
My Collection.

## CURRENT KNOWN ISSUES

Carried over from earlier testing. Most have not been re-validated against the
current working tree, so treat them as open until a fresh check is done.

- Library stale / phantom media texture issue.
- VP48 Backgammon rendering / timing / flicker (flicker persisted despite the
  confirmed final PAL timing).
- VP59+ Helicopter Rescue regression / freeze.
- G7400 / Videopac+ specific issues.
- VP61 Interpol rendering differs from MAME.
- AUTO PAL/NTSC behavior is not verified (the explicit PAL/NTSC override is
  confirmed working).
- C7010 relies on an approximate NSC800 instruction budget; exact hardware
  timing is not established.
- Audio reconstructs chip progression at frame end and does not fully timestamp
  register events (see the MCS48 Phase 1 analysis under `Development/`).
- One isolated 0030AB launch hang was reported; a rebuilt test ran and it has
  not been reproduced since.

## CURRENT DEVELOPMENT

- **MCS48-NG**: a possible replacement CPU is under study behind a controlled
  interface. The legacy `cpu.cpp` is still the active production CPU, pinned by
  the standalone conformance harness in `tests/mcs48/` (4/4 positive controls,
  12/12 expected legacy failures). Research notes live in `Development/`.
- **Beta 4 is published**: v0.31.0-beta is the current public release.
- Ongoing compatibility testing, follow-up fixes and community feedback.

---

## Updater checkpoint - 20 September 2026 (self-updater Phases 1 and 2)

Self-updater Phase 1 centralized the application version in src/version.h,
the single authoritative definition (currently v0.31.0-beta). The Windows
VERSIONINFO resource in O2EM-NG.rc is generated from it, the About page
displays it and Settings shows "Current version: v0.31.0-beta". The
executable carries numeric version 0.31.0.0 and product version string
0.31.0-beta.

Self-updater Phase 2 added a manual CHECK UPDATES button to the Settings
Updates section. Pressing it runs a GitHub release/version check on a
background worker thread and the status line reports Checking..., Up to
date, a new available version, or Could not check for updates, without
blocking the UI. VIEW RELEASE opens the public GitHub release page in the
browser; nothing is downloaded, extracted or installed. Phase 2 stays
LOOK / COMPARE / REPORT only - Phase 3 (automatic checks, downloads,
staging or file replacement) has not started.

Two Phase 2 corrections were human-QA tested and approved:

- Repeat-check crash: a completed-but-still-joinable worker thread made a
  second CHECK UPDATES press terminate the process (std::terminate over a
  joinable thread). StartCheck() now moves the finished worker under the
  update mutex and joins it outside the mutex before creating a new one; a
  currently running worker is never joined. Repeated checks run with no
  crash, abort, deadlock or freeze.
- Strict release-tag parsing: optional v/V, required numeric
  MAJOR.MINOR.PATCH, optional -suffix or +suffix. Malformed GitHub tags
  report "Unexpected GitHub version format" followed by "Could not check
  for updates" instead of silently comparing as 0.0.0.

The visible button label is CHECK UPDATES; the human-approved Settings
layout (button size and position, Updates section, status line) is
unchanged.

At the time of that checkpoint the working tree also carried temporary
O2EM-NG REGION TRACE printf logging in the frontend, launcher, emulator core
and vmachine, as diagnostic-only instrumentation for the PAL/NTSC region
investigation. That tracing has since been removed from the current tree; it
is not present in the working files now. The investigation established that an
explicit PAL selection propagates correctly through frontend -> launcher ->
emulator core -> region override -> init_system, and the final active emulation
mode is PAL (50 FPS, evblclk = 7259); the initial NTSC setvideomode call is
subsequently overridden by the explicit PAL setting. The explicit PAL/NTSC
override is working as designed. VP48 Backgammon still flickers despite the
confirmed final PAL timing and remains unresolved. Whether AUTO mode behaves
correctly remains a separate, still-open question and has not been verified.

Still unresolved, unchanged: the recurring Library stale/phantom media
texture issue, the VP48 Backgammon rendering/timing issue, the VP59+
Helicopter Rescue regression/freeze, known G7400/Videopac+ issues, and
VP61 Interpol rendering incorrectly in O2EM-NG while MAME renders it
correctly.

## UI checkpoint - 17 September 2026 (Library side panels)

The unused space right of Emulator Settings now holds two Win95 group
panels:

- Library Folders: one row per runtime folder (ROMs, Box Art, Screenshots /
  Media, Manuals, BIOS / Firmware) with an OPEN push button that opens that
  existing folder in Windows Explorer. Paths come from the existing runtime
  folder conventions via AssetManager's base path; missing folders are never
  created.
- Collection Statistics: Games, Favorites, Box Art, Screenshots, Videos and
  Manuals, counted from the existing library/media state only. No
  completeness, percentage or official-total figures.

A compact fallback (short row labels, short panel captions, footer note
hidden) takes over below a 200px comfort width, and the panels fold away
entirely below the existing 170px column threshold. Statistics split into
Library (Games, Favorites) and Media (Box Art, Screenshots, Videos, Manuals)
sections, and OPEN button text renders as normal active black label text.
Human QA approved the panels, the compact labels, both thresholds and the
footer-note behavior.

## UI checkpoint - 17 September 2026

The Library dashboard is now a complete collection workspace.

- The standalone Screenshot tab was removed and its viewer lives inside
  Cover / Media behind Box Art / Screenshots buttons, including GIF and looping
  MP4 playback and per-item DELETE to the Recycle Bin.
- The Game Library is a full-height scrollable list over the permanent catalogue
  with green/red Videopac numbers showing ROM presence, and a manually
  deletable catalogue (suppressed_catalog_entries keeps DELETE GAME DATA
  permanent).
- All actions use Win95 push buttons with pressed states.

The full catalogue lists every known game regardless of ROM presence; red
entries are legitimate missing-ROM records, not errors, and can be removed with
DELETE GAME DATA when unwanted.

Human QA passed for N/N+ separation, per-item DELETE and the 58/58+ deletion
test; the developer is reviewing remaining red entries against the real ROM
collection.

## UI checkpoint - 15 September 2026

Stage 2 implements captioned Win95 group frames and grey
information/settings/favorites panels, aligned blue metadata labels and a
separate catalogue number. Existing controls remain in place. Next: user review
in windowed/fullscreen modes before media tabs, thumbnails and search. Changes
remain local for the next beta.

<!-- Local UI checkpoint: 2026-09-14 -->
Win95 redesign, stage 1: grey workspace, navy selection and library dashboard
headings, 25% library column (max 440px), larger default window (1600x960,
desktop bounded), dashboard top area enlarged. Existing tabs and hit regions
retained. Next: assess windowed/fullscreen screenshots with Bengt against the
July 22 reference, then plan media tabs, thumbnail gallery and search. No
release published for these changes.

## Beta 4 status - 13 September 2026 (historical checkpoint)

The table and notes below are the Beta 4 baseline as of 13 September 2026
(development baseline 0030AD). They are kept for provenance; the CURRENT STATE
section at the top of this file takes precedence.

| Item | Status |
| --- | --- |
| Next version | v0.31.0-beta |
| Release name | Beta 4 |
| Release state | In preparation; not yet published |
| Latest published version | v0.30.0-beta (Beta 3), 26 July 2026 |
| Development baseline | 0030AD, retaining the user-tested 0030AC fixes |
| Platform | Windows x64, C/C++, SDL3, Visual Studio 2026 |
| Author | Bengt-Ove Peltz |

Beta 4 continues the Game Library, Import Center, BIOS management, SQLite database,
media, favorites and controller features introduced in earlier releases. Its main
new milestone is playable C7010 Chess in local testing, with stable board rendering.

## C7010 Chess

- Dedicated NSC800/Z80-compatible execution module with 8 KiB firmware and 2 KiB RAM.
- Communication latches and interleaved execution connect the module to the 8048.
- Cartridge/VDC write routing and pending timer interrupt handling were corrected.
- A relative-jump error caused incorrect display coordinates: E2-E4 could visually
  empty G2. The JR displacement is now fetched before calculating the target PC.
- The C7010 raster conversion and renderer-only character snapshot prevent the
  lower-board clipping and flicker seen in earlier test builds.
- Detailed C7010 diagnostics are off by default in Debug and Release. Firmware
  errors and unsupported-instruction reports remain available. Developers can
  rebuild with `O2EM_C7010_TRACE=1` for detailed diagnostics.

The graphics snapshot is a compatibility implementation, not proof of exact
hardware timing. NSC800 execution still uses an approximate instruction budget.

## Verified so far

- The developer played several moves with computer replies; pieces moved and
  remained visible. The 0030AC test was reported stable with no remaining flicker.
- Appearance was compared with a photograph of real C7010 hardware.
- Real firmware coordinate conversion passed for all 64 squares in both board
  orientations (128 cases), including an optimized test build.
- The 0030AD Release x64 build completed without build errors or warnings.
- Earlier fault reproduction covered both G7000 and G7400. Final Beta 4 gameplay
  should still be checked on both console configurations.

## Remaining beta validation

- Play a short game with the final Release executable before packaging.
- Test the packaged build outside the development directory and on a clean PC.
- Test longer games, captures, check/checkmate, castling, en passant and promotion.
- Test white/black selection, levels, reset, and return to the library.
- Recheck ordinary games after the shared 8048 pending-interrupt correction.
- Compare computer thinking time separately; speed tuning is deferred.
- One 0030AB launch hung and Windows recorded AppHangB1. A subsequent rebuilt test
  ran successfully; the cause of that isolated hang has not been established.

This is a community-testing milestone, not a claim of complete chess or hardware
compatibility. Use the [Beta 4 quick start guide](Beta_4_v0.31.0_Quick%20start%20guide.md)
for setup and useful bug-report details.

## Release preparation

Documentation now targets v0.31.0-beta. Release packaging now creates a
versioned folder and ZIP, including the game catalogue
and runtime DLLs. The installer reads the same package/version, and the new icon is
integrated. GitHub tag/release creation, publication and clean-PC testing remain.
Do not publish the development folder: BIOS, C7010 firmware, commercial ROMs,
manuals and copyrighted artwork are not part of the beta distribution.

## Historical project notes

The material below is preserved from earlier development. References to "current",
planned work, old BIOS filenames or completed release tests in this archive apply
to those earlier versions, not to Beta 4. The status above takes precedence.

---

# HISTORICAL DEVELOPMENT AND ARCHIVED NOTES

The material from here on is historical. It is preserved for provenance and may
describe earlier versions, old file names or completed release tests.

# Archived project notes through Beta 3

## Project Information

| Item | Value |
|------|-------|
| Project | O2EM-NG |
| Current Version | v0.30.0-beta |
| Release Name | Beta 3 |
| Started | 2026 |
| Author | Bengt-Ove Peltz |
| IDE | Visual Studio 2026 |
| Language | C / C++ |
| Platform | Windows x64 |
| Graphics | SDL3 |
| Audio | SDL3 |
| Input | SDL3 |
| Project Type | Emulator |
| Status | Public Beta / Active Development |

---

# Project Goal

O2EM-NG is a modernized Philips Videopac G7000 / Magnavox Odyssey² emulator
project based on the original O2EM emulator.

The goal is to preserve the original O2EM emulation core and original machine
behavior while replacing outdated platform dependencies with a modern
SDL3-based architecture.

The project aims to provide:

- Integrated ROM browser
- Integrated game launcher
- Modern controller support
- Keyboard support
- Two-player controller support
- Fullscreen living-room experience
- Optional windowed mode
- SDL3 video
- SDL3 audio
- SDL3 input
- Persistent settings
- PAL and NTSC region selection
- Controller-only frontend operation
- Simple Windows x64 distribution
- Community compatibility testing
- Easier maintenance
- Future platform portability
- Future display improvements
- Future compatibility improvements

The main project principle is:

**Preserve the emulation core while modernizing the platform around it.**

---

# Version (Beta 3 era - historical)

## v0.30.0-beta

Release name:

**Living Room Beta Update 1**

This version follows the first public release:

**v0.22.0-beta – Living Room Beta**

The main addition in v0.22.1-beta is controller port switching.

Xbox Y can now switch physical controller routing between the two emulated
G7000 / Odyssey² joystick ports.

This allows a user with one physical controller to play single-player games
regardless of which original joystick port the game expects.

The emulator briefly displays:

    CONTROLLER PORTS SWAPPED

or:

    CONTROLLER PORTS NORMAL

The feature has been tested successfully in:

- Bowling-Basketball single-player mode
- Gunfighter two-player multiplayer mode

---

# Status (Beta 3 era - historical)

O2EM-NG has reached public Beta status.

Working systems include:

- SDL3 video
- SDL3 window handling
- SDL3 rendering
- SDL3 audio
- SDL3 event handling
- SDL3 gamepad support
- Fullscreen startup
- Windowed startup
- ROM browser
- Game launcher
- Keyboard input
- Xbox controller input
- Left analog stick support
- D-pad support
- Fire button support
- Two physical controllers
- Multiplayer gameplay
- Return from game to frontend
- Reset during gameplay
- Controller port switching
- On-screen controller routing notification
- Integrated Settings screen
- Persistent `o2em-ng.cfg`
- Region selection
- Auto region mode
- PAL mode
- NTSC mode
- Requested and active video-mode reporting
- Windows x64 Release build
- Public source repository
- Public Beta distribution

The emulator is now being tested by users outside the original development environment.

Current development focus:

- Community feedback
- Compatibility testing
- NTSC-region testing
- Controller usability
- Four in 1 Row investigation
- Comparison with later O2EM implementations
- Careful incremental improvement

---

# User Experience (Beta 3 era - historical)

O2EM-NG is designed to behave more like a dedicated living-room console
frontend than a traditional command-line emulator.

Typical use:

1. Start O2EM-NG.
2. The ROM browser opens.
3. Select a game with keyboard or controller.
4. Launch the game.
5. Play using keyboard or Xbox-compatible controller.
6. Press Xbox Y if the game expects the opposite joystick port.
7. A brief message confirms controller routing.
8. Press Xbox B to reset the emulated machine.
9. Press Xbox Back/View to return to the ROM browser.
10. Open Settings to change display mode or region.
11. Settings are saved automatically to `o2em-ng.cfg`.

The goal is to make normal gameplay possible without requiring command-line
use or constant keyboard access.

---

# Controls

## ROM Browser

### Keyboard

- Up / Down: Move selection
- Enter: Launch selected game or open selected entry
- Esc: Exit frontend

### Controller

- D-pad Up / Down: Move selection
- Left stick Up / Down: Move selection
- A: Launch selected game or open selected entry
- B: Exit frontend

---

## Settings Screen

### Keyboard

- Up / Down: Move selection
- Left / Right: Change setting
- Enter: Select
- Esc: Return to ROM browser

### Controller

- D-pad Up / Down: Move selection
- Left stick Up / Down: Move selection
- Left / Right: Change setting
- A: Select
- B: Return to ROM browser

---

## In Game

### Keyboard

- Esc: Return to ROM browser
- F5: Reset emulated machine

### Controller

- D-pad / Left stick: Joystick movement
- A: Fire
- Xbox B: Reset emulated machine
- Xbox Back/View: Return to ROM browser
- Xbox Y: Switch controller ports

---

# Controller Architecture

O2EM-NG uses SDL3 for physical controller handling.

The input layer keeps separate state for:

- Keyboard Player 1
- Keyboard Player 2
- Gamepad buttons Player 1
- Gamepad buttons Player 2
- Gamepad axes Player 1
- Gamepad axes Player 2

These states are combined and sent to the original emulated joystick system.

This separation makes it possible to change physical controller routing
without changing the emulated hardware behavior.

---

# Original G7000 Joystick Behavior

The Philips Videopac G7000 / Magnavox Odyssey² does not have a universal rule
requiring every single-player game to use the same joystick port.

Some games expect one joystick.

Other games expect the other joystick.

This is authentic original hardware behavior.

O2EM-NG preserves this behavior.

During early controller testing, this meant that users might need two connected
controllers or might need to change which physical controller they were holding
when moving between games.

The goal became:

**Preserve authentic emulated joystick-port behavior while allowing one modern
physical controller to access either port.**

The solution was implemented entirely in the SDL3 input layer.

---

# Switch Sticks

Starting with v0.22.1-beta:

**Xbox Y switches controller ports during gameplay.**

Normal routing:

    Physical Controller 1
            |
            v
    Emulated Joystick Port 1

    Physical Controller 2
            |
            v
    Emulated Joystick Port 2

Swapped routing:

    Physical Controller 1
            |
            v
    Emulated Joystick Port 2

    Physical Controller 2
            |
            v
    Emulated Joystick Port 1

Keyboard mappings remain associated with their original emulated player states.

The emulator core itself is not modified by this feature.

Only the physical SDL3 controller routing changes.

---

# Controller Routing Notification

When Xbox Y is pressed, O2EM-NG briefly displays:

    CONTROLLER PORTS SWAPPED

or:

    CONTROLLER PORTS NORMAL

The message appears in the lower-left area of the game display and disappears automatically.

The notification is drawn after the game texture and before the final SDL presentation.

Rendering order:

    Game frame
        |
        v
    SDL texture update
        |
        v
    Render game texture
        |
        v
    Render temporary notification
        |
        v
    SDL_RenderPresent()

This provides immediate visual confirmation without interrupting gameplay.

---

# Switch Sticks Testing

## Bowling-Basketball

Test type:

**Single player**

Result:

- Game launches normally.
- One physical controller works.
- Xbox Y changes joystick-port routing.
- The same physical controller can control the required emulated joystick port.
- On-screen notification appears correctly.
- Xbox Y switches routing back to normal.

Result:

**PASS**

---

## Gunfighter

Test type:

**Two-player multiplayer**

Result:

- Two physical controllers work.
- Both players can play normally.
- Video works.
- Sound works.
- Multiplayer remains stable.
- Controller switching does not break normal two-player operation.

Result:

**PASS**

---

# Confirmed Working Features

## Video

- SDL3 renderer works.
- Original game display works.
- Fullscreen startup works.
- Windowed startup works.
- Frontend redraw after returning from game works.
- Game scaling works.
- PAL video mode works.
- NTSC video mode works.
- Controller notification overlay works.

---

## Audio

- SDL3 audio works.
- Game sound confirmed working.
- Gunfighter sound confirmed working.
- Munchkin sound confirmed working.
- Multiple games tested successfully with sound.

---

## Input

- Keyboard input works.
- Xbox 360 controller detected.
- Xbox One controller detected.
- D-pad movement works.
- Left analog stick movement works.
- Fire button works.
- Two controllers can be connected simultaneously.
- Player input works in games.
- Gunfighter multiplayer works.
- Controller port switching works.
- One physical controller can access either emulated joystick port.
- Physical routing can be changed during gameplay.

---

## Frontend

- ROM browser works.
- Game launching works.
- Controller navigation works.
- Keyboard navigation works.
- Returning from game to browser works.
- Fullscreen startup works.
- Windowed startup works.
- Settings entry works.
- Settings screen works.
- Settings supports keyboard navigation.
- Settings supports controller navigation.
- Esc returns from Settings.
- Controller B returns from Settings.
- Configuration is saved.
- Display preference is restored on startup.
- Region preference is restored on startup.

---

# Compatibility Testing

The current ROM collection has been manually tested.

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

Additional tested games also work.

The major current known compatibility exception is:

**Four in 1 Row**

---

# Gunfighter Multiplayer Milestone

Gunfighter was the first major real-world two-player controller test.

Test configuration:

- Two physical controllers
- Two players
- SDL3 gamepad input
- Game audio enabled
- Normal emulator video output

Result:

- Two-player gameplay works.
- Both controllers work.
- Sound works.
- Gameplay is stable.
- Returning to frontend works.
- Controller port switching does not break multiplayer.
- Wife-unit approval received.

This milestone was important because it demonstrated that O2EM-NG was not merely booting games.

It was actually usable as a living-room multiplayer emulator.

---

# Bowling-Basketball Controller Milestone

Bowling-Basketball became the first main single-player test of Switch Sticks.

Result:

- One physical controller used.
- Game launched normally.
- Xbox Y switched physical controller routing.
- Gameplay worked after switching.
- Notification displayed correctly.
- Switching back to normal routing worked.

This confirmed the main purpose of the feature:

**A user does not need to own two physical controllers simply because different
original games expect different joystick ports.**

---

# Four in 1 Row Investigation

Four in 1 Row currently does not work correctly in O2EM-NG.

Current behavior:

- ROM loads.
- Screen remains grey.
- Game does not reach the visible SELECT GAME screen.
- Keyboard input does not start the game.

Known facts:

- The ROM works in older O2EM implementations.
- The ROM has been confirmed working in O2EM 1.20B5.
- O2EM 1.20B5 reaches the game correctly.
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

Known ROM information:

    Size: 4096 bytes
    CRC: 3BFEF56B
    Layout: two-bank 4 KB cartridge

Historical O2EM information mentions:

- Support for Four in 1 Row.
- An `-exrom` switch.
- Special ROM mapping behavior.

Current investigation direction:

- Compare O2EM-NG with O2EM 1.20B5.
- Investigate EXROM behavior.
- Compare cartridge banking.
- Compare CRC-specific compatibility logic.
- Compare interrupt behavior.
- Compare relevant CPU behavior.
- Compare memory mapping.
- Compare relevant VDC behavior.

The current strategy is not to replace the O2EM-NG core wholesale.

Instead:

1. Find the relevant difference.
2. Understand the behavior.
3. Port only the required fix.
4. Rebuild.
5. Retest the known working game set.
6. Avoid regressions.

Status:

**Investigation open**

Internal project title:

**Four in 1 Row – The O2EM-NG Nemesis ROM**

---

# Region / PAL / NTSC Status

Region selection is implemented through the Settings system.

Available modes:

- Auto
- PAL
- NTSC

Default Beta setting:

**Auto**

---

## Auto Mode

Auto preserves the original O2EM CRC-based compatibility behavior.

Flow:

    ROM loaded
        |
        v
    Original O2EM compatibility logic
        |
        v
    Final video mode selected

Auto is the recommended default for Beta testing.

---

## PAL Mode

PAL mode allows the original compatibility logic to run and then explicitly selects PAL video mode.

Flow:

    ROM loaded
        |
        v
    Original compatibility logic
        |
        v
    Force PAL mode

PAL uses the core's 50 FPS video mode.

---

## NTSC Mode

NTSC mode allows the original compatibility logic to run and then explicitly
selects NTSC video mode.

Flow:

    ROM loaded
        |
        v
    Original compatibility logic
        |
        v
    Force NTSC mode

NTSC uses the core's 60 FPS video mode.

---

## Region Testing

Confirmed:

- Auto launches successfully.
- PAL launches successfully.
- NTSC launches successfully.
- PAL reports 50 FPS.
- NTSC reports 60 FPS.
- No obvious crashes caused by region switching during current testing.

Most development and testing is performed in Sweden.

PAL behavior is therefore easier to verify locally.

External feedback from users familiar with real Magnavox Odyssey² NTSC
behavior remains especially valuable.

Useful reports should include:

- Game name
- ROM CRC if available
- Country
- Expected region
- Selected region mode
- Reported active video mode
- Video behavior
- Speed behavior
- Sound behavior
- Controller model
- Single-player or multiplayer
- Whether Switch Sticks was needed
- Any controller problems

---

# Settings System

The Settings screen is integrated into the ROM browser.

Current settings:

- Display Mode: Fullscreen / Windowed
- Region Mode: Auto / PAL / NTSC
- Scanlines: On / Off configuration value

Settings are stored in:

    o2em-ng.cfg

Current behavior:

- Settings screen opens from ROM browser.
- Keyboard navigation works.
- Controller navigation works.
- Settings are saved.
- Display preference is restored.
- Region preference is restored.
- Scanlines preference is stored for future renderer integration.

Future possible settings:

- Controller assignment
- Per-game controller port preference
- Audio mute
- Volume
- Additional display options
- Scanline intensity
- Scaling options

---

# Runtime Folder Structure

Important runtime folders:

    O2EM-NG/
        |
        +-- BIOS/
        |
        +-- ROMS/
        |
        +-- BOXART/
        |
        +-- MANUALS/
        |
        +-- DOCS/

Users must provide their own compatible files.

O2EM-NG does not distribute:

- BIOS ROM files
- Commercial game ROMs
- Copyrighted box art collections
- Commercial manuals

The currently expected BIOS filename is:

    BIOS/o2rom.bin

---

# Development History

O2EM-NG did not begin as a plan to create a modern emulator fork.

It evolved through several stages.

The history is preserved here because many current architectural decisions came
directly from problems encountered during those earlier stages.

---

# Stage 1 – Original O2EM Frontend Experiments

The earliest goal was simply to make the existing O2EM emulator easier to use.

The original emulator worked, but the user experience was not ideal for a modern living-room setup.

Initial goals included:

- Automatic ROM discovery
- Alphabetical ROM sorting
- Favorites
- Last Played
- Playtime logging
- Keyboard navigation
- Controller navigation
- Box art
- Cleaner game selection
- Portable distribution

At this point, there was no plan to modernize the emulator itself.

The idea was to build a better frontend around the existing executable.

---

# Stage 2 – PowerShell Text Frontend

The first successful frontend was built in PowerShell.

Features included:

- Automatic ROM discovery
- Alphabetical sorting
- Favorites file
- Last Played file
- Play log
- Playtime tracking
- Two-column layout
- Arrow-key navigation
- ASCII-art title
- Exit option
- ROM count display
- Game launching

This version successfully launched games and proved that a more convenient
O2EM experience was possible.

It also established several ideas that remain part of the O2EM-NG vision:

- Browse games visually.
- Select a game easily.
- Launch without command-line use.
- Return to a frontend.
- Make the emulator comfortable for normal users.

---

# Stage 3 – PowerShell WPF Frontend

The next stage was a graphical WPF frontend.

The goal was something closer to an EmulationStation-style interface.

Experiments included:

- Graphical ROM list
- Box art
- Search
- Controller support
- Animations
- Last Played
- Favorites
- Launch button
- EXE packaging

Problems encountered included:

- PowerShell execution policy
- ps2exe module problems
- Compiled EXE behavior differences
- False error messages on exit
- Folder detection problems
- Box art path problems
- UI differences between computers
- Controls appearing differently on another machine

Despite the problems, this stage helped define the desired user experience.

---

# Stage 4 – VB.NET WinForms Frontend

The frontend was then recreated using VB.NET WinForms.

Working features included:

- ROM list
- Search
- Game launching
- Box art display
- Last Played
- Automatic game count
- Cleaner Windows application behavior

The WinForms frontend reached a stable working state.

However, the emulator still remained a separate legacy executable.

The larger question became:

**Should development continue building frontends around old O2EM, or should
O2EM itself be modernized?**

The decision was made to investigate the source code.

That decision eventually created O2EM-NG.

---

# Stage 5 – Original O2EM Source Investigation

The original O2EM source was obtained and examined.

The codebase relied on older technologies and build assumptions.

Legacy dependencies and problems included:

- Allegro
- `allegro.h`
- `winalleg.h`
- `dirent.h`
- Legacy compiler assumptions
- GCC-oriented makefiles
- Undefined identifiers
- Platform-specific code
- Old video handling
- Old input handling
- Old audio handling

Two directions were possible:

1. Recreate the old Allegro build environment.
2. Replace the legacy platform layer.

SDL3 was selected.

This was the real beginning of O2EM-NG.

---

# Stage 6 – SDL3 Decision

SDL3 was chosen as the modern platform layer.

Reasons included:

- Modern Windows support
- Video support
- Audio support
- Keyboard support
- Gamepad support
- Cross-platform potential
- Active development
- Cleaner long-term maintenance
- Possible future Linux work
- Possible future Android investigation

Initial SDL3 setup required solving:

- Include paths
- Library paths
- Linker dependencies
- DLL placement
- x64 configuration

The first standalone SDL3 test application successfully opened a window.

Milestone:

**SDL3 confirmed working.**

---

# Stage 7 – O2EM-NG Foundation

A new Visual Studio C/C++ project was created.

The project began integrating the original O2EM emulation code into a modern
Visual Studio and SDL3 environment.

Initial goals:

- Build the original CPU core.
- Load BIOS.
- Load cartridge ROMs.
- Execute the original emulation loop.
- Replace Allegro dependencies.
- Create SDL3 rendering.
- Create an integrated launcher.

The first major question was:

**Can the original O2EM core run inside a modern SDL3 application?**

The answer became yes.

---

# Stage 8 – BIOS and Cartridge Loading

The emulator core was connected to modern startup code.

Early verified systems:

- BIOS loading
- Cartridge loading
- CRC calculation
- CPU initialization
- System initialization
- CPU execution
- Frame counter activity
- Vertical blank handling

At this stage, the emulator executed code but did not yet display correct game graphics.

The CPU was alive.

The renderer was alive.

The connection between the original VDC rendering logic and SDL3 was incomplete.

Result:

**The emulator ran, but the screen was black.**

---

# Stage 9 – VDC Rendering Migration

The original O2EM VDC rendering pipeline was investigated and connected to SDL3.

Important flow:

    CPU execution
        |
        v
    VBL handling
        |
        v
    draw_region()
        |
        v
    draw_display()
        |
        v
    finish_display()
        |
        v
    SDL texture update
        |
        v
    SDL_RenderTexture()
        |
        v
    SDL_RenderPresent()

The first successful game graphics were a major milestone.

The project moved from:

    BIOS loads
    CPU executes
    screen is black

to:

    BIOS loads
    ROM loads
    CPU executes
    game graphics appear

O2EM-NG had become visibly recognizable as an emulator.

---

# Stage 10 – Integrated ROM Browser

The ROM browser was integrated directly into O2EM-NG.

This removed the need for a separate launcher application.

Features included:

- ROM discovery
- ROM list
- Keyboard navigation
- Controller navigation
- Game launching
- Return from game to browser
- Settings entry
- Box art support

Architecture changed from:

    External Frontend
        |
        v
    Launch o2em.exe

to:

    O2EM-NG
        |
        +-- ROM Browser
        |
        +-- Settings
        |
        +-- Emulator Core

This became one of the most important architectural changes in the project.

---

# Stage 11 – SDL3 Input Migration

Input handling was migrated to SDL3.

The new input system introduced separate handling for:

- Keyboard
- Gamepad buttons
- Gamepad axes
- Player 1
- Player 2

The SDL3 input layer feeds the original emulated joystick state.

Controller testing included:

- Xbox 360 controller
- Xbox One controller
- D-pad
- Analog stick
- Fire button
- Multiple controllers

The separation between physical controller state and emulated joystick state
later made Switch Sticks possible.

---

# Stage 12 – SDL3 Audio

Audio was migrated to SDL3.

Working sound was confirmed in multiple games.

Important tests included:

- Gunfighter
- Munchkin
- Additional games

At this point, O2EM-NG had:

- Video
- Audio
- Input
- CPU execution
- ROM loading
- Integrated frontend

The project had become a functional emulator.

---

# Stage 13 – Controller-Only Living Room Flow

The living-room goal required controlling the emulator without returning to the keyboard.

In-game controller shortcuts were added.

Xbox B:

    RESET EMULATED MACHINE

Xbox Back/View:

    RETURN TO ROM BROWSER

This allowed the complete flow:

1. Start O2EM-NG.
2. Select a game with controller.
3. Launch game.
4. Play game.
5. Reset with controller.
6. Return to browser with controller.
7. Select another game.

This completed the first practical controller-only living-room loop.

---

# Stage 14 – Multiplayer Milestone

Two physical controllers were connected.

Gunfighter became the main multiplayer test.

Result:

- Player 1 controller works.
- Player 2 controller works.
- Video works.
- Audio works.
- Multiplayer works.
- Returning to frontend works.
- Gameplay is stable.
- Wife-unit approval received.

This was an important real-world milestone.

The emulator was no longer only being tested by watching whether games booted.

People were actually playing games.

---

# Stage 15 – Settings System

An integrated Settings screen was added.

Settings included:

- Fullscreen / Windowed startup
- Auto / PAL / NTSC region mode
- Scanlines configuration value

Settings are stored in:

    o2em-ng.cfg

This moved the project further away from command-line configuration and toward
an appliance-style emulator experience.

---

# Stage 16 – Region Selection

Region handling was exposed through Settings.

Available modes:

- Auto
- PAL
- NTSC

Auto preserves original O2EM compatibility behavior.

PAL and NTSC explicitly select the final video mode.

Testing confirmed:

- Auto works.
- PAL works.
- NTSC works.
- PAL reports 50 FPS.
- NTSC reports 60 FPS.

External NTSC testing remains important.

---

# Stage 17 – The Nemesis Appears

During compatibility testing, most games worked.

Then came Four in 1 Row.

Symptoms:

- ROM loads.
- CPU executes.
- Screen becomes grey.
- SELECT GAME never appears.
- Keyboard input does not start the game.

Many hours of investigation followed.

Tests included:

- ROM validation
- CRC verification
- Banking investigation
- CPU tracing
- Timer investigation
- IRQ investigation
- Interrupt vector checking
- Alternate bank selection
- Inverted banking
- Sound-disabled tests
- Keyboard tests

The game remained undefeated.

Later, the same ROM was confirmed working in O2EM 1.20B5.

This provided a valuable working comparison target.

Four in 1 Row became:

**The O2EM-NG Nemesis ROM**

---

# Stage 18 – Beta Preparation

Once the emulator reached a stable playable state, work shifted toward public distribution.

Preparation included:

- Source tree cleanup
- Visual Studio project cleanup
- Release x64 configuration
- SDL3 Release include configuration
- SDL3 Release library configuration
- Linker verification
- Runtime DLL verification
- BIOS folder
- ROMS folder
- BOXART folder
- MANUALS folder
- DOCS folder
- README files
- Copyright documentation
- Git ignore rules
- Public documentation

Release configuration exposed differences between Debug and Release.

Problems included:

- Different include paths
- Different library paths
- Different linker dependencies
- Missing runtime DLLs
- Console subsystem behavior

Each problem was corrected and tested.

---

# Stage 19 – Release Build and Console Window

The Release build initially opened an unwanted console window.

Changing the subsystem exposed:

    unresolved external symbol WinMain

Startup configuration was corrected.

Final result:

- O2EM-NG starts cleanly.
- No unwanted console window.
- BIOS loads.
- Games launch.
- Emulator works normally.

The Windows x64 Release build was ready.

---

# Stage 20 – First Public Beta

The first public package was prepared as:

    O2EM-NG-v0.22.0-beta-Windows-x64

and distributed as:

    O2EM-NG-v0.22.0-beta-Windows-x64.zip

The package was tested outside the development folder.

The repository became public.

The first release was published.

Version:

**v0.22.0-beta**

Release name:

**Living Room Beta**

This marked the transition from private development project to public emulator Beta.

Users began downloading the emulator.

Community feedback began immediately.

---

# Stage 21 – Community Feedback

The first public discussions produced new ideas and technical conversations.

Topics included:

- Controller behavior
- Joystick port behavior
- Timing
- Game speed
- Android interest
- Later O2EM versions
- C7420 emulation
- Four in 1 Row compatibility
- Joystick port swapping

A later unofficial O2EM 1.20 branch became an important technical reference.

Reported features include:

- C7420 Home Computer cartridge emulation
- Z80 cartridge processor emulation
- Communication between Z80 and Videopac
- BASIC CLOAD
- BASIC CSAVE
- QuickLoad
- QuickSave
- Intelligent keyboard mapping
- Additional G7400 keyboard support
- Per-game XML configuration
- Joystick auto-detection
- Joystick swapping
- Additional debugger functions
- Various emulation fixes

These features provide several possible future research directions.

---

# Stage 22 – Authentic Controller Problem

The original G7000 joystick-port behavior became an important usability discussion.

Different games may expect different joystick ports.

O2EM-NG correctly preserved this behavior.

However, a modern user with only one physical controller could encounter a game
that expected the other emulated port.

The new goal became:

**Keep the emulated hardware authentic while making one physical controller
usable with either original joystick port.**

The correct location for this solution was the SDL3 input layer.

Not the emulator core.

---

# Stage 23 – Switch Sticks Implementation

The first implementation was deliberately simple:

    Xbox Y = Switch Controller Ports

A controller-routing state was added.

Normal:

    Controller 1 -> Joystick Port 1
    Controller 2 -> Joystick Port 2

Swapped:

    Controller 1 -> Joystick Port 2
    Controller 2 -> Joystick Port 1

The first test succeeded immediately.

**The controller ports swapped perfectly.**

---

# Stage 24 – On-Screen Notification

After Switch Sticks worked, a visual notification was added.

Messages:

    CONTROLLER PORTS SWAPPED

and:

    CONTROLLER PORTS NORMAL

The notification:

- Appears briefly
- Is rendered over the game
- Appears near the lower-left corner
- Disappears automatically
- Does not interrupt gameplay

This completed the first full Switch Sticks implementation.

---

# Stage 25 – Switch Sticks Validation

The feature was tested in both major use cases.

## Single Player

Game:

**Bowling-Basketball**

Result:

**PASS**

One controller could be switched to the required emulated joystick port.

## Multiplayer

Game:

**Gunfighter**

Result:

**PASS**

Two-controller multiplayer remained functional.

The new routing feature did not break the existing multiplayer input system.

---

# Stage 26 – v0.22.1-beta

Because users had already downloaded v0.22.0-beta, the updated build should not
silently replace the original release.

A new version was selected:

**v0.22.1-beta**

Main changes:

- Xbox Y Switch Sticks
- One-controller access to either emulated joystick port
- On-screen controller routing notification
- Bowling-Basketball single-player testing
- Gunfighter multiplayer regression testing
- Documentation updates

This became the first update following the public Beta release and community feedback.

---

# ARCHIVED EARLIER RELEASE INFORMATION

# Release History

## v0.22.0-beta – Living Room Beta

First public Beta release.

Major features:

- SDL3 video
- SDL3 audio
- Integrated ROM browser
- Integrated launcher
- Integrated Settings
- Persistent configuration
- Fullscreen mode
- Windowed mode
- Auto region mode
- PAL region mode
- NTSC region mode
- Xbox controller gameplay
- Two-controller multiplayer
- In-game reset
- Controller return to browser
- Windows x64 Release build
- Public source repository
- Public release ZIP

---

## v0.30.0-beta

Release Name: **Beta 3**

Beta 3 expands O2EM-NG from the original ROM-browser frontend into a more
complete game-library environment.

The release adds the integrated Game Library, Import Center, SQLite-backed game
database, BIOS management, media import, favorites, game information, manual
access, and live media refresh while preserving the existing SDL3 emulator core,
controller support, region handling, and living-room workflow.

The Windows x64 release package was tested outside the development environment
and on a clean Windows PC before publication.

---

## v0.22.1-beta – Living Room Beta Update 1

First public Beta update.

Major addition:

**Switch Sticks**

Features:

- Xbox Y controller port switching
- One-controller support for games expecting different joystick ports
- On-screen routing notification
- Single-player validation
- Multiplayer regression testing

---

# Development Milestones

## Frontend Proof of Concept

Completed:

- PowerShell launcher
- ROM discovery
- Game launching
- Favorites experiments
- Last Played experiments

---

## Graphical Frontend Experiments

Completed:

- WPF frontend
- Box art experiments
- Search
- Controller concepts
- EXE packaging experiments

---

## Compiled Windows Frontend

Completed:

- VB.NET WinForms frontend
- ROM browser
- Search
- Box art
- Last Played
- Game launching

---

## SDL3 Foundation

Completed:

- SDL3 window
- SDL3 renderer
- Visual Studio x64 project
- SDL3 linking
- SDL3 runtime

---

## Emulator Core Alive

Completed:

- BIOS loading
- Cartridge loading
- CRC calculation
- CPU execution
- VBL activity
- Original core integration

---

## Game Graphics

Completed:

- VDC rendering connected
- Game frames visible
- SDL3 texture presentation

---

## Playable Emulator

Completed:

- Video
- Audio
- Keyboard input
- Controller input
- ROM browser
- Game launching

---

## Living Room Operation

Completed:

- Controller frontend navigation
- In-game reset
- Return to browser
- Fullscreen startup
- Persistent settings

---

## Multiplayer

Completed:

- Two physical controllers
- Gunfighter multiplayer
- Stable video
- Stable sound
- Real gameplay testing

---

## Public Beta

Completed:

- Release x64
- Clean Windows build
- Distribution ZIP
- Public repository
- Public release
- Community downloads
- Community feedback

---

## First Beta Update

Completed:

- Switch Sticks
- Xbox Y port switching
- On-screen notification
- Single-controller convenience
- Multiplayer regression testing

---

# Architecture (historical)

High-level architecture:

    O2EM-NG
        |
        +----------------------+
        |                      |
        v                      v
    ROM Browser             Settings
        |                      |
        |                      v
        |                 o2em-ng.cfg
        |
        v
    Game Launcher
        |
        v
    EmulatorCore_StartRom()
        |
        v
    Load BIOS
        |
        v
    Load Cartridge
        |
        v
    Initialize CPU
        |
        v
    Initialize Machine
        |
        v
    Original O2EM Core
        |
        +-----------------------------+
        |                             |
        v                             v
    CPU / Memory / VDC           SDL3 Input
        |                             |
        |                             +-- Keyboard
        |                             |
        |                             +-- Gamepad Buttons
        |                             |
        |                             +-- Gamepad Axes
        |                             |
        |                             +-- Port Routing
        |                             |
        |                             +-- Reset Shortcut
        |                             |
        |                             +-- Return Shortcut
        |
        v
    draw_region()
        |
        v
    draw_display()
        |
        v
    finish_display()
        |
        v
    SDL Texture Update
        |
        v
    Render Game Texture
        |
        v
    Optional Notification Overlay
        |
        v
    SDL_RenderPresent()

The architecture intentionally separates:

- Emulated hardware behavior
- Physical platform input
- Frontend behavior
- Configuration
- Rendering presentation

This separation allows modern convenience features without unnecessary changes
to the original emulation core.

---

# O2EM 1.20 Research

A later unofficial O2EM 1.20 branch has been identified as an important technical reference.

Reported changes include:

- Updated Allegro version
- C7420 emulation
- Z80 cartridge processor emulation
- Communication between cartridge Z80 and Videopac
- Per-game XML configuration
- Intelligent keyboard routine
- Additional Videopac+ keyboard keys
- Hard reset
- Improved file browser
- Joystick auto-detection
- Joystick swapping
- MegaCart fixes
- VDC behavior changes
- Videopac+ character rendering fixes
- Expanded debugger
- Memory save tools
- Z80 memory support
- Sprite editor
- Sprite search
- Runtime speed adjustment

O2EM-NG will not simply replace its current working core with this branch.

Preferred strategy:

1. Use later O2EM versions as references.
2. Compare relevant code carefully.
3. Separate platform changes from core changes.
4. Port useful fixes individually.
5. Rebuild after each meaningful change.
6. Retest the known working game set.
7. Avoid damaging the current compatibility baseline.

---

# C7420 Research

C7420 support is an interesting possible future milestone.

The C7420 Home Computer cartridge is significantly more complex than a normal Videopac cartridge.

Research areas include:

- Z80 processor emulation
- Cartridge memory
- Communication between Z80 and G7000/G7400 system
- BASIC environment
- Keyboard mapping
- CLOAD
- CSAVE
- BASIC text files
- Binary files
- QuickLoad
- QuickSave
- File storage paths

This should be treated as a major separate emulation feature.

It should not be mixed casually into unrelated controller, frontend, or compatibility work.

---

# Planned Features

## Display

Completed:

- SDL3 renderer
- Game display
- Fullscreen startup
- Windowed startup
- Saved display mode
- Game scaling
- Notification overlay

Planned:

- Scanline effect
- Optional CRT appearance
- Shader/filter research
- Additional scaling improvements
- Possible integer scaling

---

## Audio

Completed:

- SDL3 audio
- Working game sound

Possible future improvements:

- Mute
- Volume control
- Additional synchronization testing

Audio changes are not currently urgent because the existing sound path is working.

---

## Controller

Completed:

- SDL3 gamepad support
- Xbox-compatible controller support
- D-pad
- Analog stick
- Fire button
- Two controllers
- Multiplayer
- Reset shortcut
- Return-to-browser shortcut
- Switch Sticks
- On-screen routing notification

Possible future improvements:

- Controller reconnect handling
- Controller status display
- Controller assignment UI
- Per-player controller selection
- Remappable shortcuts
- Remember preferred joystick port per game
- Automatic per-game routing

---

## Frontend

Completed:

- ROM browser
- Keyboard navigation
- Controller navigation
- Game launching
- Settings entry
- Settings screen
- Persistent configuration
- JPG box art support
- PNG box art support

Possible future improvements:

- Expanded box art presentation
- Game details panel
- Favorites
- Last Played
- Compatibility status
- Cleaner release branding
- Additional game metadata

---

## Settings

Current:

- Fullscreen / Windowed
- Auto / PAL / NTSC
- Scanlines value stored
- Persistent configuration
- Keyboard navigation
- Controller navigation

Possible future settings:

- Scanline rendering
- Controller assignment
- Per-game joystick-port preference
- Audio mute
- Volume
- Scaling options

---

# Platform Research

SDL3 gives O2EM-NG possible future portability.

Potential platforms:

- Linux
- Android

These are research directions, not current release promises.

Android is particularly interesting because:

- SDL3 supports Android development.
- The emulator core is mostly portable C/C++.
- Physical Bluetooth controllers could fit the project well.
- Touch controls could eventually be investigated.

However, platform work would require:

- Build-system work
- Storage handling
- App lifecycle handling
- Touch UI
- Mobile frontend adaptation
- Real-device testing

The current primary platform remains:

**Windows x64**

---

# Beta Testing Priorities

Current testing priorities:

1. More game compatibility reports.
2. NTSC-region feedback.
3. Speed reports.
4. Audio reports.
5. Controller model reports.
6. Single-player controller behavior.
7. Multiplayer controller behavior.
8. Switch Sticks testing.
9. Fullscreen behavior.
10. Windowed behavior.
11. Clean-machine Release testing.
12. Long gameplay sessions.

Useful reports should contain enough information to reproduce the issue.

Preferred report information:

- O2EM-NG version
- Game name
- ROM CRC if known
- Region
- Selected region setting
- Controller model
- Number of controllers
- Exact problem
- Whether problem happens every time
- Video behavior
- Sound behavior
- Speed behavior

---

# Regression Test Baseline

Before major changes to:

- CPU timing
- VDC timing
- Interrupt behavior
- Memory mapping
- Cartridge banking
- Audio timing
- Input state handling

retest the known working baseline.

Recommended games:

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

Also retest:

- Auto mode
- PAL mode
- NTSC mode
- Keyboard input
- One-controller gameplay
- Switch Sticks
- Two-controller multiplayer
- Reset shortcut
- Return-to-browser shortcut
- Audio
- Fullscreen startup
- Windowed startup
- Settings persistence

---

# Debug and Release Lessons

O2EM-NG development has shown that Debug and Release configurations must be tested independently.

A successful Debug build does not guarantee a successful Release build.

Check both configurations for:

- Include paths
- Library paths
- Additional dependencies
- Preprocessor definitions
- Runtime DLLs
- Subsystem configuration
- Working directory
- BIOS path
- ROM path
- Configuration path

Runtime dependencies must exist beside the executable where required.

For SDL3:

- `SDL3.lib` is required at link time.
- `SDL3.dll` is required at runtime.

Cleaning x64 output folders can remove required runtime DLLs.

Always verify the output folder after cleaning or rebuilding.

---

# BIOS Notes

During Release testing, a compatible BIOS had a descriptive filename.

The emulator expected:

    o2rom.bin

Renaming the BIOS solved startup.

Current expected location:

    BIOS/o2rom.bin

Future BIOS detection may become more flexible.

No BIOS ROM is distributed with O2EM-NG.

---

# Distribution Status

Completed:

- Public source repository
- Public Beta release
- Windows x64 Release build
- Clean distribution folder
- ZIP package
- BIOS folder
- ROMS folder
- BOXART folder
- MANUALS folder
- DOCS folder
- Data-folder README files
- Copyright documentation
- `.gitignore`
- Release testing outside development directory
- Community announcement
- External downloads
- First post-release feature update

Current distribution work:

- v0.22.1-beta build
- Updated documentation
- Updated source repository
- Updated Release package
- Community update notice

---

# Repository Rules

Do not distribute:

- BIOS ROM files
- Commercial game ROMs
- Copyrighted box art collections
- Commercial manuals

Do not commit:

- Local build output
- Visual Studio temporary files
- Local BIOS files
- Local ROM collections
- Private development artifacts

Preserve:

- Original O2EM attribution
- Copyright notices
- Project history
- `DOCS/COPYRIGHTS.txt`
- Version history
- Changelog accuracy

---

# Development Philosophy

O2EM-NG development follows a practical and conservative approach.

Priorities:

1. Keep the emulator enjoyable to use.
2. Preserve working compatibility.
3. Collect real community feedback.
4. Fix reproducible compatibility problems.
5. Improve controller usability.
6. Compare later O2EM versions where useful.
7. Avoid uncontrolled core rewrites.
8. Test after significant changes.

The Four in 1 Row investigation demonstrated that one compatibility problem can consume many hours.

The project therefore favors:

- Small changes
- Clear goals
- Immediate testing
- Regression testing
- Preserving working behavior

Switch Sticks is a good example:

- Small change
- Clear user benefit
- No change to emulated hardware behavior
- Implemented in SDL3 input layer
- Tested in single-player
- Regression-tested in multiplayer

---

# Lessons Learned

## Preserve Working Code

The original O2EM core represents years of emulator development.

The O2EM-NG goal is not to rewrite everything because some platform code is old.

Instead:

- Preserve working emulation logic.
- Replace obsolete platform dependencies.
- Modernize interfaces.
- Investigate compatibility issues individually.
- Compare known working implementations.

---

## Small Features Can Matter

Switch Sticks is technically small compared with CPU or VDC emulation.

Its user benefit is large.

One controller can now be used across games that expect different original joystick ports.

This demonstrates the value of improving the platform layer without changing
emulated hardware behavior.

---

## Real Gameplay Matters

A title screen is not enough.

Useful testing includes:

- Playing the game
- Testing movement
- Testing fire
- Testing sound
- Testing reset
- Returning to browser
- Multiplayer
- Longer sessions
- Controller switching

Gunfighter and Bowling-Basketball have become valuable practical test cases.

---

## Community Feedback Changes Development

The first public release immediately produced new questions and ideas.

Examples:

- Android interest
- Timing discussion
- Later O2EM investigation
- C7420 interest
- Controller-port usability

Public Beta development therefore changes the project from one development
environment into a broader compatibility effort.

---

# Open Questions (historical)

The project has moved beyond:

**Can O2EM run with SDL3?**

Answer:

**Yes.**

It has moved beyond:

**Can O2EM-NG display real game graphics?**

Answer:

**Yes.**

It has moved beyond:

**Can O2EM-NG play real games with sound and controllers?**

Answer:

**Yes.**

It has moved beyond:

**Can O2EM-NG support real two-player gameplay?**

Answer:

**Yes.**

It has moved beyond:

**Can O2EM-NG be publicly distributed as a Windows x64 Beta?**

Answer:

**Yes.**

The current questions are:

- How compatible is the full game library?
- How accurate is behavior across PAL and NTSC systems?
- Which compatibility differences exist between O2EM branches?
- How can controller usability improve without changing original hardware behavior?
- Which later O2EM fixes should be investigated?
- What will community testing reveal?

---

# Immediate Development Direction

Current version:

**v0.22.1-beta**

Immediate focus:

- Build and test Release x64.
- Update repository.
- Publish v0.22.1-beta.
- Announce Switch Sticks update.
- Collect community feedback.
- Continue NTSC testing.
- Continue compatibility testing.
- Preserve the current working game baseline.
- Revisit Four in 1 Row when ready for another battle with the Nemesis.

---

# Future Direction

Possible future areas:

- More compatibility fixes
- Per-game controller routing preferences
- Controller reconnect handling
- Scanline rendering
- CRT-style display options
- Favorites
- Last Played
- Game information panel
- Improved box art presentation
- Linux research
- Android research
- C7420 research
- Later O2EM compatibility improvements
- Additional debugger or diagnostic tools

These should be approached incrementally.

The current working emulator must remain the stable foundation.

---

# Credits

## Original O2EM

O2EM-NG is based on the original O2EM emulator.

Original O2EM authors:

- Daniel Boris
- Andre de la Rocha
- Arlindo M. de Oliveira

Their work forms the foundation of the emulation core preserved and modernized by O2EM-NG.

Original copyright and attribution information is maintained in:

    DOCS/COPYRIGHTS.txt

---

## O2EM-NG

Modern SDL3 modernization, frontend development, integration, testing,
packaging, and project development:

**Bengt-Ove Peltz**

Copyright © 2026 Bengt-Ove Peltz

---

# Final Project Principle

O2EM-NG exists to help preserve and enjoy the Philips Videopac G7000 /
Magnavox Odyssey² on modern systems.

The project should continue to modernize the experience without casually
changing the machine being emulated.

The guiding principle remains:

**Old hardware. Original games. Modern platform.**