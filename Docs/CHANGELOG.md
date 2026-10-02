# Changelog

## 0.31.0-beta "Beta 4" - Unreleased

Beta 4 has not been published yet. Everything in this section is development
toward the unreleased 0.31.0-beta; the most recent work is listed first.

### Win95 window chrome and framed banner

- The SDL window is now borderless and presented as a classic Windows 95
  window. The frontend draws a custom client-side title bar (application icon,
  white title text, minimize / maximize / close buttons) and a gray menu bar
  with File, Library, Settings, Tools and Help. The menu bar is a visual shell;
  its entries are not wired to actions yet.
- Normal window behavior is preserved on the borderless window: dragging the
  title bar moves the window, double-clicking it toggles maximize/restore, the
  three title-bar buttons minimize / maximize / close, and the window edges
  resize it. Fullscreen continues to work. There is no duplicate native title
  bar.
- The layout now reserves a 26 px title bar and a 22 px menu bar (48 px total).
  The existing `assets/O2EM-NG_Header.png` artwork is unchanged; it is now
  framed inside the UI with a 6 px gray margin at 110 px high instead of
  spanning edge to edge. The navigation row and content area moved down to
  match. See `frontend_chrome.h/.cpp`, `frontend_layout.cpp`, `frontend_tabs.cpp`
  and `frontend_panels.cpp`.

### My Collection - personal physical collection

- New dedicated page for the user's own physical Videopac collection, backed by
  its own database `GAMEDATA/mycollection.db`. It is logically separate from the
  main preservation database; the main DB is read only and never receives
  collection data.
- Add Entry, Edit Entry, Remove (with confirmation), Mark Wanted / Unmark Wanted,
  search, sortable columns, quantity and duplicate handling, collection
  statistics, and UTF-8 CSV export.
- Wanted state is visible as a dedicated "Wanted" column in the main table and
  as a compact "W" status box in the Library summary panel.
- Reference linking resolves a collection entry back to a main-library game so
  the existing title, catalogue number and box art are reused read-only. No
  cover art or metadata is copied into the collection database.
- Cartridge, box and manual condition are tracked separately with a shared
  controlled vocabulary (Mint, Near Mint, Excellent, Very Good, Good, Fair,
  Poor, Damaged, plus blank). A component that is not present stores no
  condition.
- The Notes field is a real multiline editor: one-line-height caret, UTF-8 safe
  Backspace/Delete, arrow and line navigation, Enter for new lines, scrolling,
  and mouse caret placement. Every editable field supports Ctrl+A/C/X/V with
  UTF-8 clipboard handling.
- The Library dashboard gained a compact "My Collection" summary panel with
  # / Title / Cart / Box / Manual / W columns and Owned / Boxed / Manuals totals.

### UTF-8 SDL media paths

- Media loading now converts `std::filesystem::path` values to UTF-8 before
  passing them to SDL. Previously `path::string()` produced an ANSI string while
  the SDL image APIs expect UTF-8, so files whose names contained non-ASCII
  characters (for example U+2014 EM DASH) failed to load. The change is narrow
  and SDL-facing (`frontend_boxart.cpp`, `src/frontend/frontend_screenshot.cpp`);
  it is not a migration of the database, library or path architecture to UTF-8.

### Earlier unreleased Beta 4 work


- Self-updater Phase 1 centralized the application version: src/version.h is
  now the single authoritative definition, feeding the Windows VERSIONINFO
  resource (numeric 0.31.0.0), the About page and the Settings "Current
  version: v0.31.0-beta" line. The product version string is 0.31.0-beta.

- Self-updater Phase 2 added a manual update check to the Settings screen:
  the CHECK UPDATES button queries the GitHub releases API on a background
  worker thread and the status line reports Checking..., Up to date, a new
  available version, or Could not check for updates, without blocking the
  UI. VIEW RELEASE opens the public GitHub release page in the browser;
  nothing is downloaded, extracted or installed.

- Phase 2 reliability fixes (human QA approved): a repeat CHECK UPDATES
  press no longer terminates the process - a completed-but-still-joinable
  worker is moved under the update mutex and joined outside it before a new
  one starts, and a running worker is never joined. Release tags are now
  parsed strictly (optional v, required MAJOR.MINOR.PATCH, optional
  -suffix or +suffix), so malformed tags report "Unexpected GitHub version
  format" and "Could not check for updates" instead of silently comparing
  as 0.0.0. Repeated checks verified with no crash, abort, deadlock or
  freeze; Phase 3 (automatic checks or downloads) has not started.

- Two new Win95 panels use the unused Library space right of Emulator
  Settings. Library Folders lists the runtime ROMs, Box Art, Screenshots /
  Media, Manuals and BIOS / Firmware folders with an OPEN button per row that
  opens that folder in Windows Explorer (paths reuse the existing runtime
  folder conventions and nothing is created). Collection Statistics counts
  the current library only - Games, Favorites, Box Art, Screenshots, Videos
  and Manuals - with no completeness, percentage or official-total figures.
  When the column is narrow the panels switch to a compact fallback with
  short labels and hide the footer note, and they fold away entirely when the
  layout has no room for them.

- Library Cover / Media is now the main media viewer: the standalone Screenshot
  tab was removed and its full viewer (still images, GIF animation, silent
  looping MP4 through Windows Media Foundation, Previous/Next browsing with
  counter) is reached through Box Art / Screenshots buttons under the cover.
  Mode switches and game changes stop playback cleanly. A per-item DELETE
  button removes only the currently displayed cover or screenshot, with
  confirmation, to the Recycle Bin. Import Center deletion still removes whole
  categories and is unchanged.

- Game Library became a full-height classic list: alternating rows fill the
  panel and a Win95 scrollbar (wheel, arrows, track, draggable thumb) appears
  when the collection exceeds the visible rows. Keyboard/PageUp/PageDown
  selection keeps the selected game visible. Favorites scrolls the same way and
  the "+ N more" fallback is gone.

- Win95 push buttons with pressed states replace text-styled actions: Library
  Quick Add imports (the cover action is now labelled "Import Cover..."), EDIT
  GAME DATA / SAVE / CANCEL, and a new DELETE GAME DATA. The O2EM-NG panel
  tagline now reads "The Videopac Experience".

- DELETE GAME DATA removes only the selected database/catalogue entry after an
  explicit confirmation that names the entry. ROM files, covers, screenshots,
  GIF/MP4 media, manuals, BIOS and firmware are never touched. Deleted
  catalogue entries are remembered in a suppressed_catalog_entries table so the
  permanent catalogue does not reseed them; importing the ROM again creates a
  fresh entry.

- Emulator Settings in the Library gained a Fullscreen checkbox below
  Scanlines, sharing the existing persisted fullscreen setting with the
  Settings screen; BIOS and REGION dropdowns were shortened. System Information
  now reports C7010 and C7420 NSC800 firmware status using the same detection
  as Settings.

- Catalogue identity fix: Videopac+ cartridges are separate identities (54 is
  not 54+). Historical Plus filename spellings (vp_NN+, vp_NNpl, vp_NN_12,
  vp_NN_12fix, vp_NN_16) now resolve to the same N+ catalogue entry, removing
  duplicate plain-number rows while N and N+ remain fully independent.

- ROM presence is now decided by the actual ROM directory: ALL GAMES lists the
  full permanent catalogue, and the Videopac number shows green when the exact
  ROM is installed, red when the catalogue entry has no ROM, and stays blank
  for games without a catalogue number; the Game Information number plate
  behaves the same. Missing-ROM entries stay selectable for metadata and media
  and refuse to launch with guidance; importing the ROM reactivates the
  existing entry without duplicates.

- Library makeup: number/title/favorite columns, alternating rows, vector gold
  favorite stars and row-aligned mouse selection. Full catalogue names are
  retained; long titles are shortened only for display. Narrower portrait cover
  frame and compact catalogue number field.

- Win95 UI stage 2: classic captioned group frames in Library, grey
  information/settings/favorites panels, aligned blue metadata labels and a
  distinct Videopac number field. Media and description retain white
  backgrounds. Dashboard action coordinates and emulator behavior are
  unchanged.

- Win95 UI stage 1: larger 1600x960 startup window bounded by the desktop, a
  desktop-aware minimum size, grey workspace, navy selection/headings, narrower
  library and more room for cover/information. Existing tabs and actions
  remain. Search and thumbnail gallery are planned for a later stage.

- Screenshot import and previews support MP4 videos: silent playback loops
  while visible, using Windows Media Foundation on a decoding thread. Switching
  games or tabs releases playback. Still images and animated GIFs remain
  supported.

- Screenshot view plays animated GIF previews using frame delays, looping while
  visible. Switching away releases the animation; returning restarts it. GIF
  files can be imported and discovered alongside still screenshots.

- VP31 Musician and VP40 4 in 1 Row now activate XROM mapping by CRC: fixed
  3 KiB program mapping and bounded MOVX access to the full 4 KiB cartridge.
  Includes the French VP40 variant. Gameplay verification pending.

### Initial Beta 4 preparation (13 September 2026; baseline 0030AD)

Prepared 13 September 2026; development baseline 0030AD, retaining 0030AC fixes.

### Added

- Embedded application/window icon based on the O2EM-NG emblem.
- Automatic Release folder/ZIP with SDL, PDFium, C++ runtime DLLs and game catalogue.
- Beta 4 installer sourced from that package; existing user databases are preserved.

- C7010 Chess module support with separate NSC800/Z80-compatible execution,
  8 KiB firmware, 2 KiB RAM, communication latches and interleaved CPU execution.
- Separate C7010 firmware status in Settings; firmware is supplied by the user.
- A Beta 4 quick start guide with chess setup and community testing instructions.

### Fixed

- Per-user installation allows imports and settings saves without administrator rights.
- Import source folders are remembered separately from automatic destination folders.
- Distribution includes the populated 222-record catalogue, with personal
  favorites and play history reset.

- Cartridge write routing that interfered with C7010/VDC communication.
- Lost enabled timer interrupts when the 8048 was already servicing an interrupt.
- NSC800 relative-jump target calculation that produced wrong board coordinates,
  including E2-E4 visually emptying G2.
- C7010 lower-board clipping and flickering characters during row updates.

### Changed

- Detailed C7010 diagnostics default to off in Debug and Release; important error
  messages remain. `O2EM_C7010_TRACE=1` enables developer tracing when rebuilding.

### Validation and limitations

- Several moves and computer replies tested locally; pieces remained visible and
  0030AC was reported free of flicker, with appearance compared to real hardware.
- All 128 firmware coordinate conversions passed, including with optimization.
- 0030AD Release x64 built without errors or warnings. Final Release gameplay,
  packaged/clean-PC testing and a fresh ordinary-game regression pass remain pending.
- Full games and special chess moves are not yet verified. Final testing on both
  G7000 and G7400 is requested; thinking-time accuracy is still under investigation.
- An isolated 0030AB application hang was reported; a later rebuilt test ran.
- Local package/installer preparation is included; no GitHub release has been published.

---


## 0.30.0-beta "Beta 3"

### Added

- Integrated Game Library.
- Integrated Import Center.
- SQLite-backed game database stored in `GAMEDATA/o2em-ng.db`.
- BIOS management.
- ROM import through the frontend.
- Cover-art import.
- PDF manual import.
- Screenshot import.
- Game Information display.
- Favorites support.
- Manual opening from Game Information.
- Live media refresh after imported media is added.
- Mouse support in the frontend.

### Improved

- The frontend has developed from the original ROM browser into a more complete
  library-based interface.
- Games and related media can be added from inside O2EM-NG instead of being
  managed only through File Explorer.
- Game records and media are organized through the integrated database and project folder structure.
- Box art and game information are available directly from the library.
- Existing SDL3 video, audio, input, controller, region, and emulator-core
  behavior remains preserved.
- Controller and mouse operation in the frontend have been improved.
- The Windows 95 / Philips-era visual direction has been retained for the frontend.

### Testing

- SQLite database creation and WAL mode confirmed working.
- Game Library and launcher flow tested successfully.
- Import Center flow tested with the project data folders.
- Favorites and mouse control tested successfully.
- Game Information and manual-opening workflow integrated.
- Existing emulator behavior retained after the frontend and database additions.
- Windows x64 release package tested outside the development folder.
- Release package tested on a clean Windows PC before publication.

### Release

- Version updated to `v0.30.0-beta`.
- Beta 3 Windows x64 distribution prepared for GitHub and community testing.
- BIOS files, commercial ROMs, copyrighted box artwork, screenshots, and
  commercial manuals remain excluded from the release.
- Existing project documentation and release history are preserved and continued for Beta 3.

### Notes

Beta 3 is the largest frontend expansion since the first public Beta.

The emulator core remains based on the original O2EM work, while the
surrounding Windows and SDL3 platform now provides an integrated library,
database, import workflow, media handling, and game-information system.

---


## 0.22.1-beta "Living Room Beta Update 1"

### Added

- In-game controller port switching.
- Xbox Y now switches physical gamepad routing between the two emulated
  G7000 / Odyssey² joystick ports.
- Brief on-screen controller routing notifications:
  - `CONTROLLER PORTS SWAPPED`
  - `CONTROLLER PORTS NORMAL`
- Controller routing notification is displayed briefly in the lower-left area of the game display.

### Improved

- Single-player games that use different original joystick ports can now be
  played with one physical controller.
- Users no longer need two physical controllers simply to accommodate
  game-dependent joystick-port behavior.
- Original G7000 / Odyssey² joystick-port behavior remains preserved internally
  while the SDL3 input layer handles physical controller routing.
- Physical controller routing can be changed instantly during gameplay.
- Two-controller multiplayer remains fully supported.

### Testing

- Controller port switching tested successfully in Bowling-Basketball single-player mode.
- Two-controller multiplayer retested successfully in Gunfighter.
- Controller port switching confirmed not to interfere with normal two-player controller operation.
- Xbox Y successfully toggles between normal and swapped controller routing during gameplay.
- On-screen controller routing notification confirmed working during gameplay.

### Notes

The original Philips Videopac G7000 / Magnavox Odyssey² hardware did not have
a universal standard for which joystick port a single-player game used.

Some games expect joystick port 1 while others expect joystick port 2.

O2EM-NG continues to emulate this original behavior, but version 0.22.1-beta
adds a convenience layer in the SDL3 input system. A single physical controller
can now be switched between the two emulated joystick ports by pressing Xbox Y.

This keeps the emulated machine behavior authentic while making the emulator
easier to use with modern controllers.

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
- Source layout cleaned up by removing the obsolete emulator source subfolder
  after project paths were corrected and verified.
- Release build configuration completed for Windows x64.
- Windows console/debug window removed from the Release build.

### Fixed

- In-game controller shortcut handling works through SDL gamepad events.
- Reset and return-to-browser actions are mapped correctly during emulation.
- Esc and controller B return correctly from the Settings screen to the ROM
  browser instead of exiting the frontend.
- Fullscreen OFF correctly starts O2EM-NG in a normal window instead of being
  overridden by old forced-fullscreen startup code.
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
- Four in 1 Row investigation is paused after extensive debugging and will be
  revisited through comparison with known working O2EM implementations.

### Beta Release

- Development source layout cleaned and rebuilt successfully.
- BIOS, ROMS, BOXART, MANUALS, and DOCS distribution folders are in place.
- Data folders contain explanatory README files.
- Copyright and attribution information is maintained in `DOCS/COPYRIGHTS.txt`.
- Custom `.gitignore` prevents BIOS files, ROM files, build output, and local
  development artifacts from being committed.
- Release x64 build completed.
- Release package tested outside the development folder.
- Public GitHub repository published.
- First public Beta release published.

### Notes

- Four in 1 Row has officially earned the title of first O2EM-NG nemesis ROM.
- O2EM-NG v0.22.0-beta marked the first public release of the project.
- O2EM-NG development continues through testing, compatibility investigation,
  and community feedback.