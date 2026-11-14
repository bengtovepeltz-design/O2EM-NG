# O2EM-NG

![O2EM-NG](../assets/O2EM-NG_Header.png)

A Windows x64, SDL3-based continuation of the original O2EM emulator for
Philips Videopac G7000 / G7400 and Magnavox Odyssey².

**Current release: Beta 5 / v5.0.1 UI update.** The previous published release
is [v5.0.0 (Beta 5)](https://github.com/bengtovepeltz-design/O2EM-NG/releases/tag/v5.0.0).
This update focuses on the Windows frontend and collection usability; the NG
emulation engines are unchanged.

[Downloads](https://github.com/bengtovepeltz-design/O2EM-NG/releases) |
[Quick start](Beta_5_v5.0.1_Quick%20start%20guide.md) |
[Changelog](CHANGELOG.md) |
[Project notes](Project.md)

## What's new in v5.0.1

- **Edit Game Data:** corrected mouse row activation; rendering and hit-testing
  now share the same row geometry.
- **Edit Game Data input focus:** Game Library input is locked while the editor
  is open, preventing accidental library scrolling or game changes.
- **Check Updates:** improved JSON parsing and error handling; invalid responses
  and network failures are no longer reported as successful checks.
- **About:** expanded project, O2EM heritage, NG architecture, hardware and
  preservation information.
- **Recently Played and Favorites:** the Game Library scrolls to the selected
  entry when needed.
- **START GAME:** added below DELETE in Cover / Media, using the existing launch
  function.
- **My Real Collection:** the collection page has been renamed; existing data is
  preserved.
- **Direct collection editing:** Game Details can be edited without opening the
  separate Edit Entry dialog.

Known minor limitation: the mouse wheel does not scroll inside Edit Game Data;
the Game Library remains locked while the editor is open.

The NG emulation engines, C7010 support, audio and timing are unchanged in this
UI update.

## What's new in Beta 5

- **New NG emulation core**: new MCS48 CPU, unified machine scheduler and
  timebase, new i8244/i8245 VDC, and G7000/Odyssey² convergence.
- **G7400/Videopac+ support**: EF9340/EF9341/VPP device, Plus title
  compatibility and corrected static-character orientation.
- **New NG audio device** and compatibility/regression improvements.
- **C7010 Chess as NG special hardware**, with the external NSC800 module
  attached to the NG machine.
- Existing frontend, library, database, settings and controller workflows are
  preserved.

Beta 5 v5.0.0 remains the previous public release. Its Windows 95-style window,
UTF-8 media paths and C7010 support are retained in v5.0.1, along with the
renamed My Real Collection page.

## Features

- Windows 95-style frontend: custom title bar and menu bar, framed banner,
  beveled gray panels and classic 3D buttons.
- Game Library, launcher, favorites and editable game information.
- **My Real Collection**: a separate page and database for your own physical Videopac
  games, with per-component conditions (cartridge / box / manual), a wanted
  list, search, sorting and CSV export.
- Library side panels: Library Folders (open the ROMs, Box Art, Screenshots,
  Manuals and BIOS folders) and live Collection Statistics.
- Integrated Cover / Media viewer with still images, animated GIF and silent
  looping MP4 playback, and per-item media deletion to the Recycle Bin.
- Supplied SQLite game catalogue with titles, descriptions and metadata. Recognized
  ROMs such as `vp_01.bin` populate the library with the matching information,
  and the Videopac number shows whether the ROM is installed.
- Import Center for user-supplied ROMs, box art, PDF manuals and screenshots.
- SDL3 video, audio, keyboard, mouse and Xbox-compatible controller support.
- Fullscreen/windowed operation, Settings, Auto/PAL/NTSC region selection.
- Two-controller gameplay and in-game controller-port swapping.

Navigation: Library, Edit Game Data, Import Center, Manual, Settings, About and
My Real Collection.

The catalogue is included; game ROMs, BIOS/firmware and cover/media files are not.
Media appears when the user supplies matching files.

## Getting started

1. Download an available release and install it or extract its portable ZIP.
2. Add your compatible console BIOS to `BIOS` and games to `ROMS`.
3. Run `O2EM-NG.exe`, select the console BIOS in Settings and choose a game.
4. Add optional artwork, manuals and screenshots through the Import Center.

The portable Beta 5 package includes SDL3, SDL3_image, SDL3_ttf, PDFium and the
required Microsoft C++ runtime DLLs beside the executable. Windows system runtime
components are still required. Keep the whole extracted folder together.

### C7010 Chess

In addition to the console BIOS, supply:

```text
BIOS/C7010/c7010_z80.bin   (8 KiB module firmware)
ROMS/vp_C7010.bin         (chess cartridge)
```

Start with **1 -> Y/YES -> 2** to play white at beginner level 2, then enter a move
such as **E2-E4** and press Enter. **N/NO** selects black and reverses the board.
Level **1** is tournament mode, not the easiest level; longer thinking time there
is expected. Exact hardware timing has not been established.

## Controls

| Action | Keyboard | Xbox-compatible controller |
| --- | --- | --- |
| Select in frontend | Enter | A |
| Navigate frontend | Arrow keys | D-pad / left stick |
| Reset game | F5 | B |
| Return from game to frontend | Esc | Back / View |
| Swap joystick ports | - | Y |

Chess setup and moves use the keyboard. Keyboard Y/YES and controller Y have different roles.

## Beta testing

Please test longer chess games, captures, castling, en passant, promotion, both
colours and console configurations. Ordinary-game regression testing is also welcome.
Earlier releases were tested with games including Gunfighter, Atlantis, Golf,
Munchkin and Bowling-Basketball; this is not a fresh compatibility certification
for every title in Beta 5. Four in 1 Row remains a previously recorded issue pending retest.

Reports should include version, Windows version, game/ROM name, console BIOS,
region, startup choices, exact moves or reproduction steps, and screenshots/video.
Do not attach BIOS or ROM files. Use [GitHub issues](https://github.com/bengtovepeltz-design/O2EM-NG/issues)
or the community beta discussion.

## Building and packaging

Open the Visual Studio solution/project and build **Release | x64**. SDK paths
currently refer to the developer's installed SDL libraries; adjust them for your machine.
The build creates a versioned folder and ZIP under `dist` from the explicit list in
`tools/release-manifest.json`. The supplied game catalogue is included; private
configuration and user ROM/media folders are not copied.

See [Release packaging](Release%20packaging.md) for file selection and installer building.
Debug builds do not create distribution packages. Detailed C7010 tracing is opt-in
with the build definition `O2EM_C7010_TRACE=1`.

## Credits and licensing

O2EM-NG builds on the original O2EM work by **Daniel Boris, Andre de la Rocha and
Arlindo M. de Oliveira**. SDL3 modernization, frontend integration and ongoing
O2EM-NG development: **Bengt-Ove Peltz**.

See [COPYRIGHTS.txt](COPYRIGHTS.txt) for the project licence and retained notices.
BIOS files, C7010 firmware, commercial ROMs, commercial manuals and copyrighted
artwork are not distributed. Users supply their own legally obtained files.
