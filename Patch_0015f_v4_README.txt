O2EM-NG Patch 0015f v4 - Header Aspect Ratio Fix
=================================================

Base:
  O2EM-NG(12) + Patch 0015f v3

Changes:
  - Header artwork is no longer stretched to the banner rectangle.
  - Original aspect ratio is preserved.
  - A centred, slightly upward-biased source crop keeps the main artwork visible.
  - Windowed and fullscreen modes use the same proportional rendering logic.
  - No database, emulator, import, font, or UI layout logic was changed.

Install:
  Extract this ZIP over the O2EM-NG project root and replace frontend_layout.cpp.
  Build with x64 Debug -> Rebuild Solution.
