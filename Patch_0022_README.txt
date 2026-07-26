O2EM-NG Patch 0022 - Clean Install Catalogue / First ROM Import Fix
===================================================================

Apply
-----
Copy the contents of this ZIP into the O2EM-NG project root and allow Windows
to replace the existing files. Then open O2EM-NG.slnx and Rebuild Solution.

Fixed
-----
1. A clean installation no longer starts with an empty Library merely because
   the ROMS folder contains no installed ROM files.

2. Official catalogue records from Gamelist.txt are added to GameLibrary when
   no matching installed ROM entry exists. These entries remain selectable,
   allowing the first ROM, cover, manual and screenshots to be imported.

3. Catalogue records deliberately keep an empty romPath until a ROM has been
   imported. This lets the Import Center report the ROM as Missing rather than
   pretending that a non-existent file is installed.

4. ROM import no longer replaces GameInfo::filename. The catalogue filename is
   the stable database key and remains intact even when the destination name is
   changed in the file dialog.

5. ROM deletion clears only the installed ROM path. The catalogue/database key
   is retained so the game remains visible and the ROM can be imported again.

Database safety
---------------
The patch does not delete, rebuild or replace the user's database. Existing
favorites, play statistics and user-edited metadata remain associated with the
same rom_filename key.

Visual Studio
-------------
No new .cpp or .h files were introduced. All modified source files are already
registered in both O2EM-NG.vcxproj and O2EM-NG.vcxproj.filters. Verified project
files are included in the patch so Solution Explorer remains correct after the
patch is copied over.

Modified source files
---------------------
src/database/game_database.cpp
src/frontend/frontend_app.cpp
src/library/game_info.cpp
src/library/game_info.h

Suggested clean-install test
----------------------------
1. Back up the normal GAMEDATA/o2em-ng.db.
2. Use a clean Release folder containing Gamelist.txt but no database or ROMS.
3. Start O2EM-NG.
4. Confirm that official games are visible and show ROM status Missing.
5. Select one game and import its ROM.
6. Confirm that it becomes Present and launches.
7. Delete that ROM through Import Center.
8. Confirm that the catalogue entry remains visible and can be imported again.
