O2EM-NG Patch 0022c
Import Center live status + installed BIOS selection
====================================================

Changes
-------
1. Import Center catalogue uses ASCII installation markers:
      [x] installed ROM
      [ ] ROM not installed
   This avoids unsupported Unicode checkmark glyphs in the classic Win32 font.

2. ROM status refreshes immediately after ADD and DELETE.

3. Quick Settings BIOS selector now cycles only through BIOS files that
   actually exist in the BIOS folder (*.bin and *.rom).

4. When no BIOS is installed, Quick Settings displays:
      No BIOS installed

5. Importing a BIOS refreshes the BIOS list immediately and selects the newly
   imported file.

6. Deleting a BIOS refreshes the list immediately. If the selected BIOS was
   deleted, the next installed BIOS is selected; if none remain, the selection
   is cleared.

7. Starting a game is blocked with a clear warning when no valid installed
   BIOS is selected.

8. BIOS DELETE is disabled in Import Center when the BIOS folder contains no
   BIOS files.

Visual Studio
-------------
No new source files were added. frontend_app.cpp and frontend_app.h are already
registered in O2EM-NG.vcxproj and O2EM-NG.vcxproj.filters. Both Visual Studio
project files are included in this patch for consistency.

Installation
------------
1. Close O2EM-NG and Visual Studio.
2. Extract this ZIP over the O2EM-NG project root.
3. Allow Windows to replace existing files.
4. Open the solution in Visual Studio.
5. Run Clean Solution, then Rebuild Solution.

Suggested tests
---------------
- Start with an empty BIOS folder: Quick Settings says "No BIOS installed".
- Attempt to start an installed ROM: a BIOS-required warning appears.
- Import one BIOS: it appears and is selected immediately.
- Import vp_01.bin: its Import Center marker changes from [ ] to [x].
- Delete vp_01.bin: its marker changes back to [ ].
- Import two BIOS files and cycle Quick Settings: only those files appear.
- Delete the selected BIOS and verify another installed BIOS is selected.
