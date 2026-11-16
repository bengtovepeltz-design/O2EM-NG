# O2EM-NG Beta 5 - v5.0.2 Licensing & Attribution Update Quick Start Guide

This guide covers the **Beta 5 / v5.0.2 Licensing & Attribution Update**.

The previous published release is **Beta 5 / v5.0.1**. This update focuses on
copyright notices, licensing documentation, source provenance, attribution and
credits. The NG emulation engines, C7010 support, audio, video, timing, input,
databases and collections are unchanged.

## What's new in v5.0.2

- **Original O2EM notices:** the exact original O2EM copyright notice is restored
  in `Docs/COPYRIGHTS.txt`, and modified O2EM-derived source files now carry the
  prominent modification notice required by section 3 of the Clarified Artistic
  License.
- **MAME BSD-3-Clause notice:** the complete notice, copyright holders and
  disclaimer for the adapted Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP
  portions are included in `Docs/Licenses/LICENSE.MAME-BSD-3-Clause.txt`.
- **Third-party notices:** `Docs/THIRD_PARTY_NOTICES.md` maps every third-party
  component. The missing SDL3_ttf licence and a Microsoft Visual C++ runtime
  notice are now included.
- **Source provenance:** `Docs/SOURCE_PROVENANCE.md` records each major
  component's origin, licence, upstream revision, O2EM-NG work and open
  questions.
- **Credits:** `Docs/CREDITS.md` is the canonical credits source. The application
  About, Credits and Special Thanks pages have been expanded.
- **Website:** a dedicated Credits page is available at
  https://o2em-ng.eu/credits/.
- **No emulation changes:** the NG engines, C7010, audio, video, timing, input,
  database schema and collection data are unchanged.

## Installation

1. Install the O2EM-NG Beta 5 v5.0.2 package or extract its portable ZIP.
2. Place your compatible, legally obtained console BIOS in the **BIOS** folder.
3. Place your game ROMs in **ROMS** and select the matching console BIOS in Settings.
4. Run **O2EM-NG.exe**. The frontend creates its local database and lists your games.

Example console BIOS filenames:

```text
BIOS/G7000.bin
BIOS/G7400.bin
BIOS/Odyssey2.bin
```

The Import Center can add box art, PDF manuals and screenshots. Descriptive names
help keep media organized: for example `vp_01.bin`, `01.jpg` and `01_manual.pdf`.
Check the selected game's information after importing; naming alone is not a
guarantee that every file will be identified correctly.

## Upgrading from v5.0.0 or v5.0.1

The installer uses the established application identity and user install
location. Upgrading preserves:

- the user game database and custom metadata;
- My Real Collection records;
- favorites, personal ratings and play history;
- settings, ROMs, BIOS files, media and save data.

A new installation receives the supplied catalogue seed. As with any Windows
application, keep a backup of personal data before a major upgrade.

## Beta 5 highlights

- New NG emulation core and MCS48 CPU.
- Unified machine scheduler and timebase.
- New i8244/i8245 VDC and G7000/Odyssey² convergence.
- G7400/Videopac+ EF9340/EF9341/VPP support.
- New NG audio device.
- C7010 chess as NG special hardware.
- Existing frontend, library, database and controller workflows preserved.
- Compatibility and regression improvements across standard and Plus titles.

## Window and My Real Collection

The main window uses a Windows 95-style title bar (with minimize, maximize/restore
and close) and a File / Library / Settings / Tools / Help menu bar. You can drag
the title bar to move the window, double-click it to maximize or restore, and drag
the window edges to resize.

**My Real Collection** is a separate page for your own physical Videopac games. It uses
its own database, kept apart from the main game catalogue, which stays read-only.
From this page you can:

- Add, edit and remove entries.
- Mark a title as **Wanted**.
- Search and sort, and track quantity and duplicates.
- Record separate **cartridge, box and manual** conditions.
- Link an entry to a library game so its title, number and box art are reused.
- Export the list as a CSV file.

Nothing from My Real Collection is written into the main catalogue, and no cover or
metadata is copied into it.

## C7010 Chess setup

Chess needs both the cartridge ROM and the separate **8 KiB C7010 processor firmware**,
in addition to the selected console BIOS:

```text
BIOS/
  G7000.bin                 (or your selected console BIOS)
  C7010/
    c7010_z80.bin
ROMS/
  vp_C7010.bin
```

Check the C7010 firmware status in Settings, then launch the chess cartridge.
The firmware is not a replacement for the G7000/G7400 BIOS.

### Start a game

1. At **SELECT GAME**, press **1**.
2. Press **Y / YES** to play white and move first, or **N / NO** to play black.
3. Choose the playing level.
4. Enter a move such as **E2-E4**, then press **Enter**. Wait for the computer reply.

For a quick test, use **1 -> Y -> 2**. According to the original manual, level **2**
is a beginner level with an approximate ten-second response time on the original
hardware. Emulator thinking times may differ.

**Level 1 is tournament mode, not the easiest level.** The manual describes a total
thinking-time budget of about one hour for 30 computer moves.

When you choose white, your pieces appear at the bottom, with **A-H** from left to
right and rank **1** at the bottom. Choosing black reverses the board orientation.
Keyboard **Y / YES** is a chess choice; **Xbox Y** is the controller-port shortcut.

## Controls

### Frontend

- Enter / controller A: select.
- Esc / controller B: back.

### In game

- F5 / controller B (East): reset.
- Esc / controller Back/View: return to the game library.
- Xbox Y (North): swap controller ports.
- Use the keyboard for chess setup and move entry.

## Please test

- Standard G7000 titles, including VP01, VP48, VP61 and Stone Sling.
- G7400/Videopac+ titles, including VP59+ and VP60+.
- C7010 chess: several moves, captures and longer games.
- Keyboard and Xbox-compatible controller coexistence.
- F5 reset, return to the library and controller-port swapping.
- Both PAL and NTSC configurations where applicable.

v5.0.2 is a licensing and attribution update. Report any game, BIOS, region,
controller and exact reproduction steps that differ from expected behavior.

## Reporting a problem

Include the version, Windows version, selected console/BIOS and region, game name,
startup choices (for example **1 -> Y -> 1**), exact move sequence, expected result,
and actual result. Attach a screenshot or a short video for visual issues. Mention
whether reset or return to the library worked. Do not upload BIOS or game ROM files.

## Legal

O2EM-NG does not include console BIOS files, C7010 firmware, commercial game ROMs,
commercial manuals or copyrighted artwork. Supply your own legally obtained files.

v5.0.2 release packages do not bundle gameplay screenshots or videos. The
`SCREENSHOTS` folder is created for your own media.

O2EM-NG is a mixed-licence project. See `Docs/COPYRIGHTS.txt`,
`Docs/CREDITS.md`, `Docs/SOURCE_PROVENANCE.md`, `Docs/THIRD_PARTY_NOTICES.md` and
`Docs/Licenses/`.
