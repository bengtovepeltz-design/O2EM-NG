O2EM-NG Patch 0020a - Win95 Theme Compile Fix

Fixes the two Visual Studio diagnostics at frontend_app.cpp line 1216:
- 'ButtonFace': undeclared identifier
- 'ButtonFace': is not a member of 'Win95Theme'

Cause:
The project theme exposes Win95Theme::Face, not Win95Theme::ButtonFace.

Change:
Win95Theme::ButtonFace -> Win95Theme::Face

Apply over Patch 0020.
