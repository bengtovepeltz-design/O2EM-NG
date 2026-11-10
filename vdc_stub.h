#pragma once

#include <SDL3/SDL.h>

void VDCStub_SetWindow(SDL_Window* window);
SDL_Renderer* VDCStub_GetRenderer(void);
void VDCStub_ShowTestPattern(void);
void VDCStub_ShowMessage(const char* message);
// Composite the shared overlay message into an already-presented frame.
// Used by both the legacy renderer and the NG host so port-swap feedback and
// the Debug active-core indicator use one mechanism.
void VDCStub_DrawOverlay(SDL_Renderer* renderer, const SDL_FRect& dst);

// MAME 0.289 i8244 update-on-write semantics. Enabled by default; set
// O2EM_I8244_LEGACY_RENDER=1 to restore the legacy O2EM renderer.
bool VDCStub_MameUpdateEnabled(void);

// Monotonic renderer generation. Incremented whenever the application-owned
// SDL_Renderer is created. Frontend texture caches can use this (together with
// their content key) to detect a future renderer recreation without relying on
// raw SDL_Renderer* equality, which the allocator may reuse.
int VDCStub_RendererGeneration(void);

// Development-only frame tracking for the video investigation. Disabled by
// default; the probe enables it explicitly before booting a ROM.
void VDCStub_DebugEnableFrameTracking(void);
unsigned long long VDCStub_DebugFrameSerial(void);
unsigned long long VDCStub_DebugFrameHash(void);
int VDCStub_DebugFrameNumber(void);
int VDCStub_DebugFrameA0(void);
int VDCStub_DebugFrameA3(void);
int VDCStub_DebugFrameAA(void);
void VDCStub_DebugCopyLastVdc(unsigned char* dst);
void VDCStub_DebugSaveLastFrameBmp(const char* path);
unsigned VDCStub_DebugPixelColor(int x, int y);
unsigned VDCStub_DebugPixelLayers(int x, int y);
