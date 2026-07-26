O2EM-NG Patch 0022a
====================
Installed Library / Import Catalog Separation

Purpose
-------
Patch 0022 made the complete Gamelist catalogue selectable so a clean
installation could import its first ROM. This follow-up keeps that ability,
but restores the intended meaning of Game Library: only titles with an
installed ROM are shown there.

Changes
-------
1. Game Library now shows only games whose ROM is actually installed.
2. Favorites, Recently Played and Most Played also contain installed games only.
3. Import Center temporarily exposes the complete game catalogue, including
   titles that do not yet have a ROM.
4. The left-panel heading in Import Center is now:
      GAME CATALOG - SELECT GAME TO IMPORT
5. The bottom game counter always reports installed games, not catalogue size.
6. Importing a ROM makes the title appear in Game Library.
7. Deleting a ROM removes the title from Game Library while retaining its
   catalogue/database record for later re-import.
8. No database schema change is made. Existing metadata, favorites and play
   statistics are preserved.

Visual Studio
-------------
No new source files were added. O2EM-NG.vcxproj and
O2EM-NG.vcxproj.filters are included and already contain all modified files.
No manual Add Existing Item step is required.

Installation
------------
1. Close Visual Studio and O2EM-NG.
2. Extract this ZIP over the O2EM-NG project root.
3. Allow Windows to replace existing files.
4. Open the solution in Visual Studio.
5. Run Clean Solution, then Rebuild Solution.

Clean-install test
------------------
1. Start with an empty ROMS folder.
2. Library should show 0 Games and NO GAMES IN THIS VIEW.
3. Open Import Center.
4. The full catalogue should be selectable there.
5. Import one ROM for the selected catalogue title.
6. Return to Library: only the imported game should now be listed.
7. Delete that ROM in Import Center: it should disappear from Library but
   remain selectable in the Import Center catalogue.
