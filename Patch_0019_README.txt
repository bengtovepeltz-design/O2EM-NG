O2EM-NG PATCH 0019 - LIBRARY LAYOUT TEST + EDITOR CARET
========================================================

Base:
- Install after Patch 0018.

Changed files:
- src/frontend/frontend_app.cpp
- src/frontend/frontend_app.h

Library layout test:
- Cover / Media is narrower and follows the displayed image more closely.
- A new right-side Quick Settings panel is shown in Library.
- Quick Settings contains BIOS, Region, Scanlines and Fullscreen.
- Click a Quick Settings row to cycle/change that setting immediately.
- A new Favorites panel displays up to five favorite games.
- Game Library list now uses the normal O2EM-NG UI font instead of the wide
  Videopac pixel font.
- List entries use two-digit Videopac numbers and mixed-case game titles.
- GAME LIBRARY - ALL GAMES remains as the Library heading.

Editor fix included:
- A visible white two-pixel caret is drawn while typing in the red field.
- Left/Right, Home/End, Backspace and Delete work at the caret position.
- New text is inserted at the caret instead of always being appended.

This is intentionally a layout test. No database schema changes are made.
Existing settings and game information are preserved.
