# O2EM-NG FrontendApp Refactor 01

## Added

- `src/frontend/frontend_app.h`
- `src/frontend/frontend_app.cpp`

## Changed

- `main.cpp` now handles only SDL startup, window creation, the event loop, and shutdown.
- `O2EM-NG.vcxproj` includes the new FrontendApp source and header files.

## Moved into FrontendApp

- ROM loading and official-library view creation
- Current ROM selection
- Temporary Settings menu entry
- Keyboard navigation
- Gamepad D-pad, A/B, and left-stick navigation
- Settings state and persistence
- Emulator launch and renderer restoration
- Frontend and settings rendering
- Box-art shutdown

## Expected behavior

The application should look and behave as before this refactor. This change is architectural only and prepares the frontend for real tab state, library management, favorites, recent games, and media views.

## Visual Studio test

1. Open `O2EM-NG.slnx`.
2. Confirm that `src/frontend/frontend_app.cpp` and `.h` appear in Solution Explorer.
3. Build `Debug | x64`.
4. Verify keyboard and gamepad navigation.
5. Open Settings, change each option, return, and launch a ROM.
6. Exit a ROM and confirm that the frontend renderer returns correctly.
