O2EM-NG Patch 0018 - Game Data UI Polish
==========================================

Base
----
Apply after Patch 0017.

Changes
-------
- Main tab label: Cartridge -> Game Data
- Page heading: GAME INFORMATION -> GAME DATA
- Edit-mode heading: EDIT GAME INFORMATION -> EDIT GAME DATA
- Identification label: Game / Manual ID -> Videopac No.
- ROM label: ROM file -> ROM File
- Action label: EDIT METADATA -> EDIT GAME DATA

Files
-----
frontend_tabs.cpp
src/frontend/frontend_app.cpp

Installation
------------
Copy the files into the matching locations in the O2EM-NG source tree,
replace the existing files, rebuild in Visual Studio, and test the Game Data tab.

Notes
-----
Internal enum and function names still use Cartridge/Metadata where appropriate.
Only user-facing terminology has been changed, minimizing code risk.
