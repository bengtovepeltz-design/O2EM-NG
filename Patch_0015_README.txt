O2EM-NG Patch 0015 - Beta 3 Foundation
=======================================

Apply this patch on top of O2EM-NG(10).

Changes
-------
- Added the first complete Beta 3 Import Center under the Extras tab.
- Added buttons for ROM, Manual, Cover and Screenshot import.
- Added keyboard shortcuts 1-4 in the Import Center.
- Added a central ImportManager so file dialogs, validation, destination folders,
  stable filenames and copying are handled in one place.
- Imported media is resolved immediately and shown without browsing folders.
- Added Inno Setup installer project in Installer/O2EM-NG_Setup.iss.
- Added Installer/Build Installer.bat with checks for Release build and Inno Setup.
- Installer creates all user-content folders automatically and includes the
  database, catalogue, assets, PDFium and required Release DLL files.

Import naming
-------------
ROM:         ROMS/vp_XX.bin
Manual:      MANUALS/XX_imported_manual.ext
Cover:       BOXART/XX_plastic_front.ext
Screenshot:  SCREENSHOTS/XX_screenshot_N.ext

Build test
----------
1. Extract over the O2EM-NG project root.
2. Open O2EM-NG.slnx.
3. Select x64 Release.
4. Rebuild Solution.
5. Test Extras -> Import Center with a selected game.
6. Install Inno Setup 6 and run Installer/Build Installer.bat.

Note
----
Patch 0015 deliberately introduces the shared import architecture without a
large frontend redesign. This is the first Beta 3 product-polish patch.
