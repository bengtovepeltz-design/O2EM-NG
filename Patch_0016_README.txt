O2EM-NG Patch 0016 - About and Credits
========================================

Base version:
  O2EM-NG(13).ZIP

Changes:
  - Activates the existing About renderer. The About tab previously called
    the placeholder renderer even though DrawAboutTab() already existed.
  - Reworks the About page around the O2EM-NG preservation philosophy.
  - Displays the current documented release: v0.23.0-beta - Collection Update.
  - Adds a separate Credits tab to the frontend tab strip.
  - Adds acknowledgements for original O2EM, O2EM-NG development, SDL3,
    SQLite, PDFium, Windows/Visual Studio, testers and preservation communities.
  - Points users to Docs/COPYRIGHTS.txt for legal and copyright details.

Files:
  frontend_tabs.h
  frontend_tabs.cpp
  src/frontend/frontend_app.h
  src/frontend/frontend_app.cpp

Installation:
  Copy the files into the matching folders in the O2EM-NG project and allow
  Windows to replace the existing files.

Test checklist:
  1. Build x64 Debug or Release.
  2. Open About and confirm all text is visible.
  3. Open Credits and confirm all text is visible.
  4. Change tabs with mouse, Left/Right, Tab and controller shoulder buttons.
  5. Confirm tab selection remains aligned after the new Credits entry.
