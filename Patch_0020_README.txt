O2EM-NG PATCH 0020 - BETA 3 UI COMPLETION
==========================================
Base: O2EM-NG(15).ZIP / Patch 0019 state

CHANGES
-------
1. Favorites panel is now functional
   - Reads live favorite state from the game library / SQLite-loaded data.
   - Sorts favorites alphabetically.
   - Highlights the currently selected game.
   - Single-click selects the favorite in Library.
   - Double-click selects and launches the favorite.
   - Automatically returns to ALL GAMES when a favorite is outside the
     currently filtered collection.
   - Shows "No favorites added" when empty.
   - Shows a compact "+ N more" indicator when the panel is full.

2. Quick Settings UI polish
   - Replaced plain clickable text rows with Win95-style combo-box visuals
     for BIOS and Region.
   - Added a Win95-style Scanlines checkbox.
   - Fullscreen was intentionally removed from Quick Settings and remains
     in the main Settings page.
   - Changes are saved immediately through the existing settings system.

3. Collection selection support
   - Added CollectionManager::SelectFilename() so dashboard favorites can
     reliably select a game even when another collection filter is active.

FILES CHANGED
-------------
src/frontend/frontend_app.cpp
src/frontend/frontend_app.h
src/collection/collection_manager.cpp
src/collection/collection_manager.h

INSTALL
-------
Copy the files from this patch over the matching files in the current
O2EM-NG project, then rebuild in Visual Studio.

TEST CHECKLIST
--------------
- Add/remove favorites with F or controller X.
- Confirm Favorites updates immediately.
- Click a favorite and confirm the Library selection changes.
- Double-click a favorite and confirm the game starts.
- Cycle BIOS and Region from Quick Settings.
- Toggle Scanlines from Quick Settings.
- Restart O2EM-NG and confirm Quick Settings values were saved.
- Test both windowed and fullscreen layouts.
