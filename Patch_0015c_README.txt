O2EM-NG Patch 0015c - Screenshot module path fix
=================================================

Base:
  O2EM-NG(10) + Patch 0015 + 0015a + 0015b

Fixes:
- Moves frontend_screenshot.cpp/.h into src/frontend.
- Corrects the include path used by frontend_app.cpp.
- Updates O2EM-NG.vcxproj and O2EM-NG.vcxproj.filters.
- Changes the empty screenshot hint from "Extras" to "Import Center".

Install:
1. Close Visual Studio.
2. Extract this ZIP over the O2EM-NG project root.
3. Allow files to be overwritten.
4. Open O2EM-NG.slnx.
5. Select x64 Debug.
6. Rebuild Solution.

Expected result:
- frontend_screenshot.h is found.
- frontend_screenshot.cpp appears in the Visual Studio project.
- Screenshot display remains active.
