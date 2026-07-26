O2EM-NG Patch 0013a
====================

Hotfix for Patch 0013 compile errors:
- Adds missing metadata_text_editor.h include to frontend_app.cpp.
- Fixes Win64 HMENU control-ID casts.
- Fixes LONG/int mismatch in std::max during WM_SIZE.

Install over the project after Patch 0013, then Rebuild Solution.
