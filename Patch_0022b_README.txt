O2EM-NG Patch 0022b - Import Catalog Installation Markers
==========================================================

Changes
-------
- Import Center catalogue now shows a clear ROM installation marker:

    [ ] 06  Tenpin Bowling/Basketball
    [✓] 08  Baseball

- [✓] means that a ROM is installed for the title.
- [ ] means that the title is available in the catalogue but its ROM is not installed.
- Game Library behaviour is unchanged: it still lists installed games only.
- The bottom game counter still reports installed games only.
- No database schema or user metadata is changed.

Visual Studio
-------------
No new source files were introduced. The existing Visual Studio project and
filter files are included in the patch so the project remains ready to open
and rebuild without manual Solution Explorer changes.

Installation
------------
1. Close Visual Studio and O2EM-NG.
2. Extract this ZIP over the O2EM-NG project root.
3. Allow Windows to replace the existing files.
4. Open the solution in Visual Studio.
5. Run Clean Solution, then Rebuild Solution.
