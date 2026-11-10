#pragma once
// ============================================================================
// src/mcs48/g7400_freeze_trace.h
//
// Opt-in, bounded, observational diagnostic for the Helicopter Rescue+
// (vp_59+.bin, CRC32 C4134DF8) area-transition freeze.
//
// Purpose: capture what the emulated machine is doing when the game visibly
// freezes, so the failure can be classified as:
//   A. infinite/repeating CPU polling loop
//   B. CPU stopped / invalid state
//   C. repeated waiting for VDC status
//   D. repeated waiting for collision/status
//   E. waiting for an IRQ/event that never occurs
//   F. renderer frozen while the CPU continues
//   G. other identifiable state
//   H. unresolved
//
// It is OBSERVATIONAL ONLY. It never writes emulation state, never adds CPU
// cycles, never changes timing/VDC/VPP/collision semantics, and never performs
// an extra register read. When the environment variable is unset it is inert.
//
// Environment:
//   O2EM_G7400_FREEZE_TRACE   set and non-empty => enable (armed only when the
//                             running cartridge CRC is C4134DF8).
//
// Dump trigger:
//   - press F9 during gameplay to write the log immediately, and/or
//   - the log is also written once when the emulated game exits normally.
//
// Output: g7400_freeze_trace.log in the process working directory.
// ============================================================================

namespace mcs48
{
// True when the environment variable is set (regardless of CRC match).
bool G7400FreezeTrace_EnvEnabled();

// Called by EmulatorCore_StartRom once the cartridge CRC and BIOS are known.
void G7400FreezeTrace_Configure(const char* romPath, unsigned long romCrc,
    const char* biosFile, const char* regionText, int vpp);

// F9 hotkey: write the captured history immediately (no-op unless armed).
void G7400FreezeTrace_RequestDump();

// Called when the emulated game exits: write the log once if not already done,
// then uninstall the observation callbacks.
void G7400FreezeTrace_Shutdown();

// Small observational hooks that cannot be derived from the generic
// observation events.
void G7400FreezeTrace_NoteA2PreClear(); // collision snapshot before clear
void G7400FreezeTrace_NoteFrameEnd();

// Record a VDC A0 write exactly as the production path processes it.
// oldA0 = VDCwrite[0xA0] before the write; newA0 = the written value;
// yBefore/yAfter = y_latch around the production latch handling (yAfter is
// read after the original latch code ran, before VDCwrite[A0] is stored).
void G7400FreezeTrace_NoteA0Write(unsigned oldA0, unsigned newA0,
    unsigned yBefore, unsigned yAfter);

// VDC address 0x24 activity (purely observational; the production read/write
// already happened, no extra bus access).
void G7400FreezeTrace_NoteVdc24Read(unsigned returned, unsigned branch);
void G7400FreezeTrace_NoteVdc24Write(unsigned requested, unsigned before,
    unsigned after, bool blocked);

// Synthetic, opt-in dump-robustness self-test (no emulation, no gameplay).
// Returns true (and fills result) only when O2EM_G7400_FREEZE_SELFTEST is set.
bool RunFreezeTraceSelfTestIfRequested(int& result);
}
