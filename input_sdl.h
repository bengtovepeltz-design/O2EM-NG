#pragma once

#include <SDL3/SDL.h>

void InputSDL_HandleEvent(const SDL_Event& event);

// Open controllers that were already present before the event loop started.
// This does not change bindings or mapping; it only ensures the existing input
// layer has handles for already-connected devices.
void InputSDL_EnsureGamepadsOpen(void);

// Refresh the existing button/axis logical state from the open gamepads.
// Used by the NG host once per frame as a lossless fallback for missed events.
void InputSDL_RefreshGamepadStates(void);

// Clear held button/axis/keyboard state without touching physical device
// handles. Used on focus loss and at game-session boundaries.
void InputSDL_ClearHeldState(void);

// Full session reset: clears held state and returns the controller port swap
// toggle to its normal default. Physical gamepads stay open.
void InputSDL_ResetSessionState(void);