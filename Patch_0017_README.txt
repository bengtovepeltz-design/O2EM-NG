O2EM-NG Patch 0017 - Project Information System
================================================

Base: O2EM-NG(13) with Patch 0016

Replace the included files using the same folder structure, then rebuild the
Visual Studio project. No new source files need to be added to Solution Explorer.

Included project pages
----------------------
- About
- Credits
- Contributors
- Special Thanks
- Roadmap
- Release Notes

What changed
------------
- Adds the SQLite table project_pages to GAMEDATA/o2em-ng.db.
- Creates default rows only when they are missing (INSERT OR IGNORE).
- Existing user edits therefore survive later starts and updates.
- About and Credits now use one shared, scrollable Project Information view.
- All six pages can be selected from buttons inside the view.
- The existing native metadata text editor is reused for editing.
- Edit with the Edit button, Enter/A, or keyboard E.
- Save and Cancel work exactly as in the Game Information editor.
- Mouse wheel scrolls long project pages.
- Page Up/Page Down changes the selected project page.
- Opening the main About tab selects About; opening Credits selects Credits.

Database safety
---------------
The patch does not replace GAMEDATA/o2em-ng.db. The new table and default rows
are created by the program at startup. Do not overwrite an existing user database
when creating future installers or update packages.

Suggested test
--------------
1. Build and start O2EM-NG.
2. Open About and select every project page.
3. Edit Contributors, save, leave the tab and return.
4. Restart O2EM-NG and verify the change remains.
5. Test mouse-wheel scrolling and Page Up/Page Down.
6. Open Credits from the main tab strip and verify Credits is selected.
