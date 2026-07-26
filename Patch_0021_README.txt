O2EM-NG Patch 0021 - Header restoration after gameplay
======================================================

Base project:
  O2EM-NG(16).ZIP

Problem fixed:
  After returning from a game, the O2EM-NG header could become completely
  black or display corrupted/stretched graphics.

Root cause:
  The emulator's close_display() destroys the SDL renderer. The frontend header
  texture was cached globally and could remain as a stale pointer. If SDL later
  reused the same renderer address, the existing renderer-pointer comparison did
  not always detect that the cached texture was invalid.

Fix:
  FrontendLayout_Shutdown() is now called immediately before LaunchRom(), while
  the frontend renderer is still valid. This safely destroys and resets the
  cached header texture. On return from gameplay, RefreshRenderer() obtains the
  new renderer and the next frontend redraw reloads O2EM-NG_Header.png.

Files changed:
  src/frontend/frontend_app.cpp

Installation:
  Copy the included src folder into the O2EM-NG project root and allow Windows
  to replace the existing file. Rebuild in Visual Studio.

Test checklist:
  1. Start O2EM-NG and confirm the header is displayed normally.
  2. Launch a game and return to the frontend.
  3. Repeat at least 10 times with different games.
  4. Test both scanlines ON/OFF and windowed/fullscreen if available.
  5. Confirm the header never becomes black or corrupted.
  6. Confirm Favorites X and joystick-port swap Y still work normally.
