# O2EM-NG Beta 4 - v0.31.0-beta Quick Start Guide

This guide covers the published v0.31.0-beta (Beta 4) release.
Beta 4 adds C7010 Chess support and fixes the misplaced/disappearing pieces and
flickering board seen during development. Longer games still need community testing.
It also adds a Windows 95-style window, a personal **My Collection** page, and a
media-loading fix for file names that contain non-ASCII characters.

## Installation

1. Install the Beta 4 package or extract its portable ZIP.
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

## Window and My Collection

The main window uses a Windows 95-style title bar (with minimize, maximize/restore
and close) and a File / Library / Settings / Tools / Help menu bar. You can drag
the title bar to move the window, double-click it to maximize or restore, and drag
the window edges to resize. Those menu entries are a visual shell in this beta and
are not wired to actions yet.

**My Collection** is a separate page for your own physical Videopac games. It uses
its own database, kept apart from the main game catalogue, which stays read-only.
From this page you can:

- Add, edit and remove entries.
- Mark a title as **Wanted**.
- Search and sort, and track quantity and duplicates.
- Record separate **cartridge, box and manual** conditions.
- Link an entry to a library game so its title, number and box art are reused.
- Export the list as a CSV file.

Nothing from My Collection is written into the main catalogue, and no cover or
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
The firmware is not a replacement for the G7000/G7400 BIOS. Beta feedback for both
console configurations is welcome.

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

- F5: reset.
- Esc: return to the Game Library.
- Xbox Y: swap controller ports.
- Use the keyboard for chess setup and move entry.

## Please test

- Several consecutive moves, captures and longer games without disappearing pieces.
- White and black play, different levels, reset and return to the library.
- Castling, en passant, promotion, check and checkmate, if you know chess.
- G7000 and G7400, plus your usual non-chess games.

Detailed diagnostic logging is disabled by default. The beta does not yet claim
complete chess validation or exact hardware timing.

## Reporting a problem

Include the version, Windows version, selected console/BIOS and region, game name,
startup choices (for example **1 -> Y -> 1**), exact move sequence, expected result,
and actual result. Attach a screenshot or a short video for flicker. Mention whether
reset or return to the library worked. Do not upload BIOS or game ROM files.

## Legal

O2EM-NG does not include console BIOS files, C7010 firmware, commercial game ROMs,
commercial manuals or copyrighted artwork. Supply your own legally obtained files.
