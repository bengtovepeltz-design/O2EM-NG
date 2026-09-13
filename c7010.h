#pragma once

#include "types.h"

// Philips C7010 Chess Module foundation.
// The cartridge ROM (vp_C7010.bin) runs on the normal 8048.
// The external module contains an NSC800/Z80-compatible CPU, 8 KiB ROM,
// 2 KiB RAM and two 8-bit communication latches.

bool C7010_IsEnabled();
bool C7010_ConfigureForCartridge(unsigned long cartridgeCrc);
bool C7010_LoadFirmware(const char* path);
void C7010_Reset();
// 0030Q: observe 300 execution slices after ENTER.
void C7010_ArmMoveTrace();

void C7010_WriteP1(Byte value);

// 0030S: execute a share of the frame budget after each 8048 instruction.
void C7010_Run8048Cycles(unsigned int cycles, unsigned int frameCycles);
// Frame-level communication trace bookkeeping (does not execute instructions).
void C7010_RunDiagnosticStep();

// 8048-side cartridge bus hooks. Return true when C7010 handled the access.
bool C7010_ExternalRead(ADDRESS address, Byte& value);
bool C7010_ExternalWrite(ADDRESS address, Byte value);

// Patch 0030K diagnostic hook: records the 8048 MOVX bus before any
// C7010/VDC routing takes place in vmachine.cpp.
void C7010_Trace8048ExternalAccess(bool isWrite, ADDRESS address, Byte value);

// Diagnostic/future CPU interface.
Byte C7010_NSC800_ReadMemory(unsigned short address);
void C7010_NSC800_WriteMemory(unsigned short address, Byte value);
Byte C7010_NSC800_In(Byte port);
void C7010_NSC800_Out(Byte port, Byte value);

// 0030V: sampled video diagnostics after ENTER.
bool C7010_TraceVideoFrame();
void C7010_TraceVdcWrite(unsigned int address, Byte value, bool blocked, int clock);

// 0030W: bounded sample in frame 30 after ENTER.
bool C7010_TakeRasterSample();
void C7010_TraceBoardWrite(unsigned int address, Byte oldValue, Byte value);

// 0030AD: opt-in detailed diagnostics in either build configuration.
#ifndef O2EM_C7010_TRACE
#define O2EM_C7010_TRACE 0
#endif
