#pragma once
// Phase 2B: null callbacks by default. No device reads and no CPU state writes.
// Kept header-only so the unchanged standalone Phase 2A link remains valid.
#include <cstdint>
namespace mcs48::observation {
inline void (*before)(unsigned, unsigned) = nullptr;
inline void (*advance)(unsigned) = nullptr;
inline void (*event)(unsigned, unsigned, unsigned) = nullptr;
inline bool (*stop)() = nullptr;
inline bool (*input)(int) = nullptr;
inline void Before(unsigned pc, unsigned op) { if (before) before(pc, op); }
inline void Advance(unsigned cycles) { if (advance) advance(cycles); }
inline void Event(unsigned kind, unsigned address, unsigned value) { if (event) event(kind, address, value); }
// Values 0..10 are Phase 2A TraceEventKind; 11 is BUS read, 12 is input.
inline bool Stop() { return stop && stop(); }
bool RunFixtureIfRequested(int& result);
}
