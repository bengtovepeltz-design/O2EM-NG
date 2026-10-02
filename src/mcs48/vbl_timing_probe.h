#pragma once
// ============================================================================
// src/mcs48/vbl_timing_probe.h
//
// Phase 2C prep: development-only PAL VBL timing probe.
//
// Reports the OBSERVED first VBL IRQ machine cycle, first vector instruction
// and frame period for a clean PAL boot, next to the emulator constants and
// the documented MAME 0.289 PAL reference (first VBL ~7924, first vector
// 7926). Measurement only: it never changes emulator timing or IRQ behavior.
//
// Disabled unless enabled through the environment; with the trigger variable
// unset this returns false immediately and production execution is unchanged.
// See vbl_timing_probe.cpp for the environment contract.
// ============================================================================

namespace mcs48 {

// Returns false for a normal launch (probe not requested). Returns true when
// the probe was requested and ran, in which case `result` is the process exit
// code (0 = measured, 2 = could not run).
bool RunVblTimingProbeIfRequested(int& result);

} // namespace mcs48
