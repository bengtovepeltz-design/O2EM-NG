O2EM-NG Patch 0022d - Import and Media Live Refresh
===================================================

Changes
-------
1. ROM save dialog now derives the suggested destination name from the
   currently selected catalogue record. Selecting title 02 suggests vp_02.bin
   instead of reusing the previously imported ROM name.

2. Cover art, manual and screenshot status refresh immediately after import
   and deletion. No restart or selection change is required.

3. Cover texture, manual preview and screenshot caches are invalidated after
   media changes, then the current catalogue/library selection is rebuilt.

4. The ROM button always displays IMPORT, whether the ROM is missing or present.
5. Importing when a ROM is already present asks for confirmation before overwrite.

Visual Studio
-------------
No new source files were added. The existing project and filter files are
included and already reference all changed source files.

Installation
------------
Copy the contents of this patch over the O2EM-NG project root and allow files
to be replaced. In Visual Studio run Clean Solution, then Rebuild Solution.

SDL3 confirmation hotfix:
- Replaced Win32 MessageBoxA(HWND, ...) with SDL_ShowMessageBox().
- Fixes SDL_Window* to HWND compile error at frontend_app.cpp line 1984.
- No remains the safe/default cancellation choice (Escape also cancels).
