#pragma once
// Existing O2EM-NG in-game host commands.
//
// This is the NG host equivalent of the legacy handle_vbl() event branch:
// ESC / window close exits to the frontend, F5 resets the running machine,
// gamepad East resets and gamepad Back returns to the frontend. No new
// binding is introduced and no user mapping is changed.

#include <SDL3/SDL.h>

namespace o2em::ng::host {

class HostCommandState {
public:
    void HandleEvent(const SDL_Event& event) {
        if (event.type == SDL_EVENT_QUIT) {
            quit_ = true;
            return;
        }

        if (event.type == SDL_EVENT_KEY_DOWN) {
            if (event.key.key == SDLK_ESCAPE)
                quit_ = true;
            else if (event.key.key == SDLK_F5)
                reset_ = true;
            return;
        }

        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN) {
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)
                reset_ = true;
            else if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
                quit_ = true;
        }
    }

    bool QuitRequested() const { return quit_; }

    // Reset is edge-triggered: one request per physical press.
    bool TakeReset() {
        const bool value = reset_;
        reset_ = false;
        return value;
    }

private:
    bool quit_ = false;
    bool reset_ = false;
};

} // namespace o2em::ng::host
