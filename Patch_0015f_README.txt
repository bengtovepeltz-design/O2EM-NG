O2EM-NG Patch 0015f - Win95 UI Polish
======================================
Base: O2EM-NG(12).ZIP

Changes
-------
- Uses the installed Windows Tahoma font for normal interface text.
- Falls back to Segoe UI, Microsoft Sans Serif or Arial if Tahoma is unavailable.
- Keeps the original Videopac glyph font exclusively in Game Library / All Games.
- Restyles the top tab strip with classic raised and pressed Win95 borders.
- Makes Import Center compact: category, status, Add and Delete columns.
- Uses the Windows UI font in the status bar.
- No database or import behavior changes.

Test
----
1. Open O2EM-NG.slnx.
2. Select x64 Debug.
3. Rebuild Solution.
4. Check Library, Import Center, Screenshot, Manual, Settings and About.
