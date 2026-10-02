// ============================================================================
// tests/mcs48/mcs48_stub_machine.cpp
//
// MCS48-NG Phase 2A: stub machine environment for the standalone conformance
// harness. Compiled ONLY into the test target (tests/mcs48), never into the
// emulator. It links the REAL, unmodified production cpu.cpp against inert
// definitions of every machine-level symbol cpu.cpp references, so the
// harness executes the actual instruction bodies and scheduler tail exactly
// as in production.
//
// All stubs are inert: reads return neutral values (0x00/0xFF where the
// pull-up convention applies), writes are discarded, peripheral functions
// do nothing. No SDL, no VDC rendering, no audio, no C7010 execution.
// ============================================================================

#include <cstdio>
#include <cstring>

#include "types.h"
#include "vmachine.h"
#include "vpp.h"
#include "vdc.h"
#include "c7010.h"
#include "cpu.h"

// ---------------------------------------------------------------------------
// Memory (vmachine.h externs). rom is a pointer in production; the harness
// owns a fixed 4 KiB program space. Peripheral-visible arrays exist so the
// CPU can address them but are inert.
// ---------------------------------------------------------------------------
static Byte g_rom[4096];
Byte* rom = g_rom;
Byte* megarom = nullptr;
Byte intRAM[256];
Byte extRAM[256];
Byte extROM[4096];
Byte VDCwrite[256];
Byte rom_table[8][4096];
Byte coltab[256];
Byte ColorVector[500];   // MAXLINES
Byte AudioVector[500];   // MAXLINES

// ---------------------------------------------------------------------------
// Machine clocks and state (vmachine.h externs). The harness resets these
// between tests; the CPU observes them exactly as in production.
// ---------------------------------------------------------------------------
Byte dbstick1 = 0, dbstick2 = 0;
int last_line = 0;
int evblclk = 0;
int master_clk = 0;
int int_clk = 0;
int h_clk = 0;
int mstate = 0;
int frame = 0;
int key2[128];
int key2vcnt = 0;
unsigned long clk_counter = 0;
int enahirq = 0;
int pendirq = 0;
int useforen = 0;
long regionoff = 0;
int sproff = 0;
int tweakedaudio = 0;

struct resource app_data;

// ---------------------------------------------------------------------------
// Machine functions (vmachine.h). Inert: they exist to satisfy the linker.
// ---------------------------------------------------------------------------
Byte read_P2(void) { return 0xFF; }
int snapline(int /*pos*/, Byte /*reg*/, int /*t*/) { return 0; }
void ext_write(Byte /*dat*/, ADDRESS /*adr*/) {}
Byte ext_read(ADDRESS /*adr*/) { return 0xFF; }
void handle_vbl(void) {}
void handle_evbl(void) {}
void handle_evbll(void) {}
Byte in_bus(void) { return 0xFF; }
void write_p1(Byte /*d*/) {}
Byte read_t1(void) { return 0; }
void init_system(void) {}
void O2EM_SetRegionModeOverride(int /*mode*/) {}
void init_roms(void) {}
void run(void) {}
int savestate(char* /*filename*/) { return 0; }
int loadstate(char* /*filename*/) { return 0; }
size_t sys_fread(void* ptr, size_t size, size_t nitems, FILE* /*stream*/) {
    std::memset(ptr, 0, size * nitems);
    return nitems;
}

// ---------------------------------------------------------------------------
// VPP / 8243 expansion port (vpp.h). Inert.
// ---------------------------------------------------------------------------
Byte read_PB(Byte /*p*/) { return 0xFF; }
void write_PB(Byte /*p*/, Byte /*val*/) {}

// ---------------------------------------------------------------------------
// VDC / 8244-8245 (vdc.h). Only draw_region is referenced by cpu.cpp.
// ---------------------------------------------------------------------------
void draw_region(void) {}

// ---------------------------------------------------------------------------
// Voice module (voice.h). cpu.cpp polls it; the harness has no voice unit.
// ---------------------------------------------------------------------------
int get_voice_status(void) { return 0; }

// ---------------------------------------------------------------------------
// C7010 chess module (c7010.h). Fully inert: no NSC execution, no traces.
// ---------------------------------------------------------------------------
bool C7010_IsEnabled() { return false; }
bool C7010_ConfigureForCartridge(unsigned long /*cartridgeCrc*/) { return false; }
bool C7010_LoadFirmware(const char* /*path*/) { return false; }
void C7010_Reset() {}
void C7010_ArmMoveTrace() {}
void C7010_WriteP1(Byte /*value*/) {}
void C7010_Run8048Cycles(unsigned int /*cycles*/, unsigned int /*frameCycles*/) {}
void C7010_RunDiagnosticStep() {}
bool C7010_ExternalRead(ADDRESS /*address*/, Byte& /*value*/) { return false; }
bool C7010_ExternalWrite(ADDRESS /*address*/, Byte /*value*/) { return false; }
void C7010_Trace8048ExternalAccess(bool /*isWrite*/, ADDRESS /*address*/, Byte /*value*/) {}
Byte C7010_NSC800_ReadMemory(unsigned short /*address*/) { return 0; }
void C7010_NSC800_WriteMemory(unsigned short /*address*/, Byte /*value*/) {}
Byte C7010_NSC800_In(Byte /*port*/) { return 0; }
void C7010_NSC800_Out(Byte /*port*/, Byte /*value*/) {}
bool C7010_TraceVideoFrame() { return false; }
void C7010_TraceVdcWrite(unsigned int /*address*/, Byte /*value*/, bool /*blocked*/, int /*clock*/) {}
bool C7010_TakeRasterSample() { return false; }
void C7010_TraceBoardWrite(unsigned int /*address*/, Byte /*oldValue*/, Byte /*value*/) {}
