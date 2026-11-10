/*
 *   O2EM Free Odyssey2 / Videopac+ Emulator
 *
 *   Created by Daniel Boris <dboris@comcast.net>  (c) 1997,1998
 *
 *   Developed by Andre de la Rocha   <adlroc@users.sourceforge.net>
 *             Arlindo M. de Oliveira <dgtec@users.sourceforge.net>
 *
 *   http://o2em.sourceforge.net
 *
 *
 *
 *   Main O2 machine emulation
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include "audio.h"
#include "types.h"
#include "cpu.h"
#include "src/mcs48/legacy_observation.h"
#include "src/mcs48/g7400_freeze_trace.h"
#include "keyboard.h"
#include "config.h"
#include "debug.h" 
#include "vdc.h" 
#include "vdc_stub.h"
#include "vpp.h"
#include "timefunc.h"
#include "voice.h"
#include "vmachine.h"
#include "o2em_keyboard_bridge.h"
#include "o2em_keys.h"
#include "timing_sdl.h"
#include "timefunc_sdl.h"
#include <SDL3/SDL.h>
#include "input_manager.h"
#include "c7010.h"

static Byte x_latch, y_latch;
static int romlatch = 0;
static Byte line_count;
static int fps = FPS_NTSC;
static int region_mode_override = -1; // -1 = auto, 0 = NTSC, 1 = PAL

static Byte snapedlines[MAXLINES + 2 * MAXSNAP][256][2];

static int reset_requested = 0;

int evblclk = EVBLCLK_NTSC;

struct resource app_data;
int frame = 0;
Byte dbstick1, dbstick2;

int int_clk; /* counter for length of /INT pulses */
int master_clk; /* Master clock */
int h_clk; /* horizontal clock */
unsigned long clk_counter;
int last_line;
int key2vcnt = 0;
int mstate;

int pendirq = 0;
int enahirq = 1;
int useforen = 0;
long regionoff = 0xffff;
int mxsnap = 2;
int sproff = 0;
int tweakedaudio = 0;

// --- production reference-derived PAL vertical raster timebase --------------
// Standard PAL machines (G7000/Videopac with the O2 BIOS, and G7400/Videopac+
// with the G7400 BIOS) use the I8245 PAL geometry: 456 px/line, 18 px per
// 8048 machine cycle, 313 lines/frame, VBL at line 242 pixel 365. NTSC keeps
// the historical model; the French C52 and Jopac use different XTAL ratios and
// also keep the historical model. O2EM_PAL_RASTER_LEGACY_TIMING=1 forces the
// old PAL vertical timing for A/B regression testing (diagnostic, OFF default).
int pal_vertical_timing_v2 = 0;
static int pal_vbl_cycle = 6151;
static int pal_frame_base = 7929;
static int pal_frame_current = 7929;
static int pal_frame_remainder = 0;

// Unified video geometry (Checkpoint B). The definition lives here so the
// beam/status helpers below can share it; the functions are defined after the
// PAL timing configuration block.
struct UnifiedVideoGeometry
{
    int pxPerCycle;
    int htotal;
    int vtotal;
    int framePixels;
    int vblPixel;
    int hblankStart;
    int hblankEnd;
    int vblankStart;
    int vblankEnd;
    int bgateStart;
    int phaseInitPixels;
    int frameBase;
    int frameRemainderDiv;
};

static UnifiedVideoGeometry g_unifiedGeom = {
    18, 456, 313, 142728, 242 * 456 + 365, 366, 454, 242, 312, 414,
    -24, 7929, 3
};

static long long g_unifiedPhase = 0;
static int g_unifiedVblCross = 0;
static int g_unifiedFrameCross = 0;
static int g_unifiedFrameCurrent = 7929;
static int g_unifiedFrameRemainder = 0;
static int g_unifiedTest = -1;

// i8244 0xA1 horizontal-status bit from the exact PAL beam (MAME 0.289):
//   h >= 225 && h < 414 && get_y_beam() <= 242
// NTSC keeps the historical h_clk approximation when the unified model is off.
static int I8244HStatusBit(int master)
{
    if (VM_UnifiedVideoTestEnabled())
    {
        const int h = VM_UnifiedVideoHPos();
        const int y = VM_UnifiedVideoYBeam();
        return (h >= 225 && h < g_unifiedGeom.bgateStart &&
                y <= g_unifiedGeom.vblankStart) ? 1 : 0;
    }

    if (!pal_vertical_timing_v2)
        return (h_clk < (LINECNT - 7)) ? 1 : 0;

    const long long px = static_cast<long long>(master) * 18;
    const int x = static_cast<int>(px % 456);
    int y = static_cast<int>(px / 456);
    int yBeam = y;
    if (y == 0 && x < 366)
        yBeam = 313;
    if (x >= 414)
        yBeam = y + 1;
    if (yBeam > 263)
        yBeam = 263;
    return (x >= 225 && x < 414 && yBeam <= 242) ? 1 : 0;
}

int VM_PalVerticalTimingActive(void) { return pal_vertical_timing_v2; }
int VM_PalVblCycle(void) { return pal_vbl_cycle; }
int VM_PalFrameBase(void) { return pal_frame_base; }

int VM_EffectiveVblClk(void)
{
    return pal_vertical_timing_v2 ? pal_vbl_cycle : VBLCLK;
}

int VM_EffectiveFrameClk(void)
{
    if (VM_UnifiedVideoTestEnabled())
        return g_unifiedFrameCurrent;
    return pal_vertical_timing_v2 ? pal_frame_current : evblclk;
}

void VM_RasterTimingFrameBoundary(void)
{
    if (VM_UnifiedVideoTestEnabled())
    {
        g_unifiedFrameRemainder = (g_unifiedFrameRemainder + 1) %
            g_unifiedGeom.frameRemainderDiv;
        g_unifiedFrameCurrent = g_unifiedGeom.frameBase +
            (g_unifiedFrameRemainder == g_unifiedGeom.frameRemainderDiv - 1 ? 1 : 0);
    }
    if (!pal_vertical_timing_v2)
        return;
    // 313*456/18 = 7929 + 1/3 machine cycles. Integer schedule: 7929 with one
    // 7930 cycle every three frames (average 7929.333), no floating point.
    pal_frame_remainder = (pal_frame_remainder + 1) % 3;
    pal_frame_current = pal_frame_base + (pal_frame_remainder == 2 ? 1 : 0);
}

unsigned int VM_PalA4BeamValue(int master)
{
    if (VM_UnifiedVideoTestEnabled())
        return static_cast<unsigned int>(VM_UnifiedVideoYBeam());
    // Reference PAL beam: floor(master * 18 / 456) = floor(master * 3 / 76).
    int y = static_cast<int>((static_cast<long long>(master) * 18) / 456);
    return (y > 263) ? 263u : static_cast<unsigned int>(y);
}

unsigned int VM_A4BeamValue(int master)
{
    if (VM_UnifiedVideoTestEnabled())
        return static_cast<unsigned int>(VM_UnifiedVideoYBeam());
    if (!pal_vertical_timing_v2)
    {
        int y = master / 22;
        return (y > 241) ? 0xFFu : static_cast<unsigned int>(y);
    }
    return VM_PalA4BeamValue(master);
}

// Video raster line placement. The historical path keeps master/22 with the
// caller's rounding bias; the corrected PAL path uses the same vertical
// timebase as A4/VBL/frame. Audio line indexing deliberately stays at
// master/22 (sound timing is unchanged in this candidate).
static int RenderBeamLine(int master, double bias)
{
    if (VM_UnifiedVideoTestEnabled())
        return VM_UnifiedVideoBeamLine();
    if (!pal_vertical_timing_v2)
        return static_cast<int>(static_cast<float>(master) / 22.0 + bias);
    return static_cast<int>((static_cast<long long>(master) * 18) / 456);
}

void VM_ConfigurePalRasterTiming(void)
{
    const char* legacy = getenv("O2EM_PAL_RASTER_LEGACY_TIMING");
    const int legacyOn = (legacy && *legacy) ? 1 : 0;

    const int isPal = (fps == FPS_PAL) ? 1 : 0;
    const int standardPalMachine =
        (app_data.bios == ROM_O2 || app_data.bios == ROM_G7400) ? 1 : 0;

    // Derived from FujiNet/MAME PAL geometry:
    //   456 px/line, 18 px/machine cycle, 313 lines/frame, VBL line 242.
    //   VBL   = (242*456 + 365) / 18 = 6150.944 -> 6151 (nearest integer)
    //   Frame = 313*456 / 18         = 7929.333 -> 7929/7930 schedule
    // The 365 is the chip's hblank_start-1 (line 242 pixel 365).
    pal_vbl_cycle = 6151;
    pal_frame_base = 7929;
    pal_frame_current = pal_frame_base;
    pal_frame_remainder = 0;

    pal_vertical_timing_v2 = (isPal && standardPalMachine && !legacyOn) ? 1 : 0;

    if (legacyOn && isPal)
    {
        printf("O2EM-NG: PAL vertical raster timing: LEGACY model forced "
            "(O2EM_PAL_RASTER_LEGACY_TIMING).\n");
        fflush(stdout);
    }
    else if (pal_vertical_timing_v2)
    {
        printf("O2EM-NG: PAL vertical raster timing: reference 8245 model "
            "(456x313, 25.333 cycles/line, VBL 6151, frame 7929.333).\n");
        fflush(stdout);
    }
    else if (isPal && !standardPalMachine)
    {
        printf("O2EM-NG: PAL vertical raster timing: legacy model retained for "
            "this non-standard PAL machine (C52/Jopac XTAL ratio).\n");
        fflush(stdout);
    }

    VM_UnifiedVideoConfigure();
}

// ---------------------------------------------------------------------------
// Experimental unified video timebase (Checkpoint B)
// ---------------------------------------------------------------------------
// One continuous pixel phase is the authoritative beam for T1, VBL/IRQ,
// A1/A4/A5 and the raster renderer. Geometry is selected once from the
// machine/region, not from the cartridge CRC. Off by default; enable with
// O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1.

bool VM_UnifiedVideoTestEnabled(void)
{
    if (g_unifiedTest < 0)
    {
        const char* env = getenv("O2EM_UNIFIED_VIDEO_TIMEBASE_TEST");
        g_unifiedTest = (env && *env && *env != '0') ? 1 : 0;
    }
    return g_unifiedTest == 1;
}

static int UnifiedVideoIsPal(void)
{
    if (region_mode_override == 1) return 1;
    if (region_mode_override == 0) return 0;
    if (app_data.bios == ROM_O2) return 0;
    if (app_data.bios == ROM_G7400) return 1;
    return (fps == FPS_PAL) ? 1 : 0;
}

void VM_UnifiedVideoReset(void)
{
    long long p = g_unifiedGeom.phaseInitPixels % g_unifiedGeom.framePixels;
    if (p < 0) p += g_unifiedGeom.framePixels;
    g_unifiedPhase = p;
    g_unifiedVblCross = 0;
    g_unifiedFrameCross = 0;
}

void VM_UnifiedVideoConfigure(void)
{
    if (!VM_UnifiedVideoTestEnabled())
        return;

    if (UnifiedVideoIsPal())
    {
        // i8245 PAL: 456 px/line, 313 lines, 18 px/MCS48 cycle.
        // VBL = line 242, h=365 -> pixel 110717. HBlank 366..453.
        g_unifiedGeom = { 18, 456, 313, 142728, 242 * 456 + 365, 366, 454,
                          242, 312, 414, -24, 7929, 3 };
    }
    else
    {
        // i8244 NTSC: 455 px/line, 263 lines, 20 px/MCS48 cycle.
        // VBL = line 242, h=365 -> pixel 110475. HBlank 366..452.
        g_unifiedGeom = { 20, 455, 263, 455 * 263, 242 * 455 + 365, 366, 453,
                          242, 0, 413, 0, 5983, 4 };
    }

    g_unifiedFrameCurrent = g_unifiedGeom.frameBase;
    g_unifiedFrameRemainder = 0;
    VM_UnifiedVideoReset();
    printf("O2EM-NG: unified video timebase: %s %dx%d, %d px/cycle, frame %d px (%d cycles)\n",
        UnifiedVideoIsPal() ? "PAL" : "NTSC",
        g_unifiedGeom.htotal, g_unifiedGeom.vtotal, g_unifiedGeom.pxPerCycle,
        g_unifiedGeom.framePixels, g_unifiedGeom.frameBase);
    fflush(stdout);
}

void VM_UnifiedVideoAdvance(int cycles)
{
    if (!VM_UnifiedVideoTestEnabled() || cycles <= 0)
        return;
    const long long before = g_unifiedPhase;
    long long after = before + static_cast<long long>(cycles) * g_unifiedGeom.pxPerCycle;
    if (before <= g_unifiedGeom.vblPixel && g_unifiedGeom.vblPixel < after)
        g_unifiedVblCross = 1;
    if (after >= g_unifiedGeom.framePixels)
    {
        g_unifiedFrameCross = 1;
        after %= g_unifiedGeom.framePixels;
    }
    g_unifiedPhase = after;
}

int VM_UnifiedVideoPhasePixels(void) { return static_cast<int>(g_unifiedPhase); }
int VM_UnifiedVideoHPos(void) { return static_cast<int>(g_unifiedPhase % g_unifiedGeom.htotal); }
int VM_UnifiedVideoVPos(void)
{
    return static_cast<int>((g_unifiedPhase / g_unifiedGeom.htotal) % g_unifiedGeom.vtotal);
}

int VM_UnifiedVideoHBlank(void)
{
    const int h = VM_UnifiedVideoHPos();
    return (h >= g_unifiedGeom.hblankStart && h < g_unifiedGeom.hblankEnd) ? 1 : 0;
}

int VM_UnifiedVideoVBlank(void)
{
    const int h = VM_UnifiedVideoHPos();
    const int v = VM_UnifiedVideoVPos();
    const int start = g_unifiedGeom.vblankStart;
    const int end = g_unifiedGeom.vblankEnd;
    if ((v == start && h >= g_unifiedGeom.hblankStart - 1) ||
        (v == end && h <= g_unifiedGeom.hblankStart - 1))
        return 1;
    if (end < start)
        return (v > start || v < end) ? 1 : 0;
    return (v > start && v < end) ? 1 : 0;
}

int VM_UnifiedVideoT1Level(void)
{
    return (VM_UnifiedVideoVBlank() || VM_UnifiedVideoHBlank()) ? 1 : 0;
}

int VM_UnifiedVideoBeamLine(void) { return VM_UnifiedVideoVPos(); }

int VM_UnifiedVideoYBeam(void)
{
    const int h = VM_UnifiedVideoHPos();
    int v = VM_UnifiedVideoVPos();
    if (v == 0 && h < g_unifiedGeom.hblankStart)
        v = g_unifiedGeom.vtotal;
    if (h >= g_unifiedGeom.bgateStart)
        v++;
    return (v > 263) ? 263 : v;
}

int VM_UnifiedVideoXBeam(void) { return VM_UnifiedVideoHPos() >> 1; }
int VM_UnifiedVideoTakeVblCross(void)
{
    const int r = g_unifiedVblCross;
    g_unifiedVblCross = 0;
    return r;
}
int VM_UnifiedVideoTakeFrameCross(void)
{
    const int r = g_unifiedFrameCross;
    g_unifiedFrameCross = 0;
    return r;
}

Byte rom_table[8][4096];

Byte intRAM[64];
Byte extRAM[256];
Byte extROM[4096];
Byte VDCwrite[256];
Byte ColorVector[MAXLINES];
Byte AudioVector[MAXLINES];
Byte *rom;
Byte *megarom;

int key2[128];

static unsigned int key_map[6][8] = {
    { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7 },
    { KEY_8, KEY_9, 0, 0, KEY_SPACE, KEY_SLASH, KEY_L, KEY_P },
    { KEY_PLUS_PAD, KEY_W, KEY_E, KEY_R, KEY_T, KEY_U, KEY_I, KEY_O },
    { KEY_Q, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K },
    { KEY_A, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_M, KEY_STOP },
    { KEY_MINUS, KEY_ASTERISK, KEY_SLASH_PAD, KEY_EQUALS, KEY_Y, KEY_N, KEY_DEL, KEY_ENTER }
};

static void do_kluges(void);
static void setvideomode(int t);

void run(void)
{
    while (!key_done) {
        if (reset_requested) {
            reset_requested = 0;

            init_cpu();
            init_system();
        }

        if (key_debug) {
            app_data.debug = 1;
            set_textmode();
            mute_audio();
            mute_voice();
            debug();
            grmode();
            app_data.debug = 0;
            init_keyboard();
            init_sound_stream();
        }

        cpu_exec();

        // 0030S: frame-level diagnostics; CPU execution is interleaved in cpu_exec.
        C7010_RunDiagnosticStep();
    }

    close_audio();
    close_voice();
    close_display();

    
}

void handle_vbl(void)
{
    SDL_Event event;

    while (SDL_PollEvent(&event))
    {
        if (event.type == SDL_EVENT_QUIT)
            key_done = 1;

        if (event.type == SDL_EVENT_KEY_DOWN)
        {
            if (event.key.key == SDLK_ESCAPE)
                key_done = 1;

            if (event.key.key == SDLK_F5)
                reset_requested = 1;

            // Opt-in freeze diagnostic (Helicopter Rescue+): F9 dumps the
            // captured history. No-op unless the trace is armed.
            if (event.key.key == SDLK_F9)
                mcs48::G7400FreezeTrace_RequestDump();
        }

        if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        {
            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_EAST)
            {
                printf("GAMEPAD B: RESET\n");
                reset_requested = 1;
            }

            if (event.gbutton.button == SDL_GAMEPAD_BUTTON_BACK)
            {
                printf("GAMEPAD BACK: RETURN TO BROWSER\n");
                key_done = 1;
            }
        }

        InputManager_HandleEvent(event);
    }

    SDL_PumpEvents();

    if (!app_data.debug) {
        handle_key();
        update_audio();
        update_voice();
    }

    draw_region();
    mcs48::observation::Event(13, 1, 0);
    CPU_VBL_StatusLatch();
    // The CPU scheduler asserts the persistent VBL IRQ line at the event
    // point (PAL beam crossing / NTSC threshold), not here.
    {
        static int legacyVblIrq = -1;
        if (legacyVblIrq < 0)
        {
            const char* e = getenv("O2EM_I8244_LEGACY_VBL_IRQ");
            legacyVblIrq = (e && *e && *e != '0') ? 1 : 0;
        }
        if (legacyVblIrq)
            ext_IRQ();
    }
    mstate = 1;
}


void handle_evbl(void)
{
    static long last = 0;
    static int rest_cnt = 0;
    int i;

    O2EM_Delay(0);

    i = (15 * app_data.speed / 100);
    rest_cnt = (rest_cnt + 1) % (i < 5 ? 5 : i);

    if (rest_cnt==0) O2EM_Delay(1);

    last_line = 0;
    master_clk -= VM_EffectiveFrameClk();
    VM_RasterTimingFrameBoundary();
    audio_generate_frame(fps, evblclk / 22);
    frame++;
    mcs48::G7400FreezeTrace_NoteFrameEnd();
    if (!app_data.debug) {
        finish_display();
    }

    if (app_data.crc == 0xA7344D1F) {
        for (i = 0; i < 140; i++) {
            ColorVector[i] = (VDCwrite[0xA3] & 0x7f) | (p1 & 0x80);
            AudioVector[i] = VDCwrite[0xAA];
        }
    }/*Atlantis*/
    else
        for (i = 0; i < MAXLINES; i++) {
            ColorVector[i] = (VDCwrite[0xA3] & 0x7f) | (p1 & 0x80);
            AudioVector[i] = VDCwrite[0xAA];
        }

    if (key2vcnt++ > 10) {
        key2vcnt = 0;
        for (i = 0; i < 128; i++)
            key2[i] = 0;
        dbstick1 = dbstick2 = 0;
    }
    if (app_data.limit) {
        long d, f;
        d = (TICKSPERSEC * 100) / (app_data.speed * fps);
        f = ((d - gettimeticks() + last) * 1000) / TICKSPERSEC;
        if (f > 0) {

            f = f-(f%10);
            if (f>5) O2EM_Delay(f - 5);


        }
        while (gettimeticks() - last < d);
        last = gettimeticks();
    }
    mstate = 0;
}

void handle_evbll(void)
{
    static long last = 0;
    static int rest_cnt = 0;
    int i;


    O2EM_Delay(0);

    i = (15 * app_data.speed / 100);
    rest_cnt = (rest_cnt + 1) % (i < 5 ? 5 : i);

    if (rest_cnt==0) O2EM_Delay(1);


    /******************* 150 */

    for (i = 150; i < MAXLINES; i++) {
        ColorVector[i] = (VDCwrite[0xA3] & 0x7f) | (p1 & 0x80);
        AudioVector[i] = VDCwrite[0xAA];
    }

    if (key2vcnt++ > 10) {
        key2vcnt = 0;
        for (i = 0; i < 128; i++)
            key2[i] = 0;
        dbstick1 = dbstick2 = 0;
    }
    if (app_data.limit) {
        long d, f;
        d = (TICKSPERSEC * 100) / (app_data.speed * fps);
        f = ((d - gettimeticks() + last) * 1000) / TICKSPERSEC;
        if (f > 0) {

            f = f-(f%10);
            if (f>5) O2EM_Delay(f-5);


        }
        while (gettimeticks() - last < d)
            ;
        last = gettimeticks();
    }
    mstate = 0;
}

void O2EM_SetRegionModeOverride(int mode)
{
    region_mode_override = mode;
}

void O2EM_RequestReset(void)
{
    reset_requested = 1;
}

void init_system(void)
{
    int i, j, k;

    last_line = 0;
    dbstick1 = 0x00;
    dbstick2 = 0x00;
    mstate = 0;
    master_clk = 0;
    h_clk = 0;
    line_count = 0;
    itimer = 0;
    clk_counter = 0;

    app_data.limit = 1;
    app_data.speed = 100;

    init_roms();
    for (i = 0; i < 256; i++) {
        VDCwrite[i] = 0;
        extRAM[i] = 0;
    }
    for (i = 0; i < 64; i++) {
        intRAM[i] = 0;
    }
    for (i = 0; i < MAXLINES; i++)
        AudioVector[i] = ColorVector[i] = 0;

    for (i = 0; i < MAXLINES + 2 * MAXSNAP; i++)
        for (j = 0; j < 256; j++)
            for (k = 0; k < 2; k++)
                snapedlines[i][j][k] = 0;

    for (i = 0; i < 128; i++)
        key2[i] = 0;
    key2vcnt = 0;
    if (app_data.euro)
        setvideomode(1);
    else
        setvideomode(0);
    do_kluges();

    // AUTO keeps the original O2EM CRC compatibility behavior.
    // Explicit PAL/NTSC is enforced after CRC-based video-mode rules.
    if (region_mode_override == 1)
        setvideomode(1);
    else if (region_mode_override == 0)
        setvideomode(0);

    init_vpp();
    clear_collision();
}

void init_roms(void)
{
    rom = rom_table[0];
    romlatch = 0;
}

Byte read_t1(void)
{
    /*17*/
    if (VM_UnifiedVideoTestEnabled())
        return static_cast<Byte>(VM_UnifiedVideoT1Level());
    if ((h_clk > 16) || (master_clk > VM_EffectiveVblClk()))
        return 1;
    else
        return 0;
}

void write_p1(Byte d)
{
    if ((d & 0x80) != (p1 & 0x80)) {
        int i, l;
        l = snapline(RenderBeamLine(master_clk, 0.1), VDCwrite[0xA3], 1);

        for (i = l; i < MAXLINES; i++)
            ColorVector[i] = (VDCwrite[0xA3] & 0x7f) | (d & 0x80);
    }

    mcs48::observation::Event(5, 1, d);
    p1 = d;
    C7010_WriteP1(d);

    if (app_data.bank == 2) {
        rom = rom_table[(~p1) & 0x01];   /* Normal 4K banking */
    }
    else if (app_data.bank == 3) {
        rom = rom_table[~p1 & 0x03];
    }
    else if (app_data.bank == 4) {
        rom = rom_table[(p1 & 1) ? 0 : romlatch];
    }
}

Byte read_P2(void)
{
    int i, si, so, km;
    // 0030P: arm only on a real ENTER press observed while row 5 is
    // selected.  Re-arm after physical key release so a held key cannot
    // generate hundreds of trace windows.
    static bool c7010EnterArmed = true;
    const bool c7010EnterDown =
        O2EMKeyboard_IsKeyPressed(KEY_ENTER) || key2[KEY_ENTER];
    if (!c7010EnterDown)
        c7010EnterArmed = true;

    if (!(p1 & 0x04)) {
        si = (p2 & 7);
        so = 0xff;

        if (si < 6) {
            for (i = 0; i < 8; i++) {
                km = key_map[si][i];

                static int keyMatrixTrace = -1;
                static int keyMatrixFrame = -1;
                if (keyMatrixTrace < 0)
                {
                    const char* env = getenv("O2EM_KEY_MATRIX_TRACE");
                    keyMatrixTrace = (env && *env && *env != '0') ? 1 : 0;
                    const char* fenv = getenv("O2EM_KEY_MATRIX_FRAME");
                    if (fenv && *fenv)
                        keyMatrixFrame = static_cast<int>(strtol(fenv, nullptr, 10));
                }
                if (keyMatrixTrace && frame == keyMatrixFrame && km &&
                    (O2EMKeyboard_IsKeyPressed(km) || key2[km]))
                {
                    fprintf(stderr,
                        "KEYMATRIX frame=%d clk=%d pc=%03X row=%d col=%d key=%d\n",
                        frame, master_clk, unsigned(pc), si, i, km);
                }

                if (O2EMKeyboard_IsKeyPressed(km))
                {
                    //printf("O2 key pressed: matrix=%d bit=%d keycode=%u\n", si, i, km);
                }
                if (O2EMKeyboard_IsKeyPressed(km) || key2[km]) {
                    so = i ^ 0x07;
                }
            }
        }

        if (so != 0xff) {
            p2 = p2 & 0x0F;
            p2 = p2 | (so << 5);

            if (O2EM_C7010_TRACE && C7010_IsEnabled() && si == 5 && c7010EnterDown && c7010EnterArmed) {
                c7010EnterArmed = false;
                printf("O2EM-NG: C7010 0030P keyboard matrix ENTER row=%d bit=7 P1=%02X P2=%02X\n",
                    si, (unsigned int)p1, (unsigned int)p2);
                CPU_ArmC7010EnterTrace(p2);
            }
        }
        else {
            p2 = p2 | 0xF0;
        }
    }
    else {
        p2 = p2 | 0xF0;
    }

    return p2;
}

static Byte ext_read_impl(ADDRESS adr);
Byte ext_read(ADDRESS adr)
{
    const Byte value = ext_read_impl(adr);
    static int openBusTrace = -1;
    if (openBusTrace < 0)
    {
        const char* env = getenv("O2EM_OPENBUS_TRACE");
        openBusTrace = (env && *env && *env != '0') ? 1 : 0;
    }
    if (openBusTrace && adr == 0xA2 && (p1 & 0x40))
    {
        static int openBusA2Logs = 0;
        if (openBusA2Logs < 8)
        {
            ++openBusA2Logs;
            fprintf(stderr,
                "OPENBUS_A2 value=%02X p1=%02X p2=%02X exrom=%d megaxrom=%d vdcgate=%d\n",
                unsigned(value), unsigned(p1), unsigned(p2),
                app_data.exrom, app_data.megaxrom,
                (!(p1 & 0x08) && !(p1 & 0x40)) ? 1 : 0);
        }
    }
    mcs48::observation::Event(2, adr, value);
    if (!(p1 & 0x08) && !(p1 & 0x40))
        mcs48::observation::Event(8, adr, value);
    static const bool traceVp31 = getenv("O2EM_TRACE_VP31") != nullptr;
    static unsigned samples = 0;
    if (traceVp31 && app_data.crc == 0xAFB23F89 && pc == 0x280 && samples < 24)
    {
        ++samples;
        printf("VP31 BIOS WAIT PC=%03X adr=%02X value=%02X P1=%02X P2=%02X VDC06=%02X\n",
            unsigned(pc), unsigned(adr), unsigned(value), unsigned(p1), unsigned(p2), unsigned(VDCwrite[6]));
        fflush(stdout);
    }
    return value;
}

static Byte ext_read_impl(ADDRESS adr)
{
    // Patch 0030K: observe the real 8048 MOVX read before C7010/VDC routing.
    // 0xFF is the pre-routing/open-bus value; returned data is still logged by
    // the C7010 latch trace when the module actually claims the access.
    C7010_Trace8048ExternalAccess(false, adr, 0xFF);

    Byte c7010Value = 0xFF;
    if (C7010_ExternalRead(adr, c7010Value))
        return c7010Value;

    Byte d;
    Byte si;
    Byte m;
    int i;

    if (!(p1 & 0x08) && !(p1 & 0x40)) {
        /* Handle VDC Read */
        switch (adr) {
        case 0xA1:
            d = VDCwrite[0xA0] & 0x02;
            if (CPU_VblStatus())
                d = d | 0x08;
            if (I8244HStatusBit(master_clk))
                d = d | 0x01;
            if (sound_IRQ)
                d = d | 0x04;
            mcs48::observation::Event(15, 0xA1, d);
            sound_IRQ = 0;
            {
                static int a1Trace = -1;
                if (a1Trace < 0)
                {
                    const char* e = getenv("O2EM_IRQ_TRACE");
                    a1Trace = (e && *e && *e != '0') ? 1 : 0;
                }
                if (a1Trace)
                    fprintf(stderr, "A1_READ frame=%d mclk=%d pc=%03X p1=%02X p2=%02X irqLine=%d d=%02X\n",
                        frame, master_clk, unsigned(pc), unsigned(p1), unsigned(p2),
                        CPU_IrqLineState(), unsigned(d));
            }
            CPU_IrqLineAck();
            if (CPU_IsrTraceActive())
                fprintf(stderr, "ISR_R adr=A1 d=%02X (post-ack) p1=%02X p2=%02X frame=%d mclk=%d\n",
                    unsigned(d), unsigned(p1), unsigned(p2), frame, master_clk);
            return d;
        case 0xA2:
            si = VDCwrite[0xA2];
            m = 0x01;
            d = 0;
            for (i = 0; i < 8; i++) {
                if (si & m) {
                    if (coltab[1] & m)
                        d = d | (coltab[1] & (m ^ 0xFF));
                    if (coltab[2] & m)
                        d = d | (coltab[2] & (m ^ 0xFF));
                    if (coltab[4] & m)
                        d = d | (coltab[4] & (m ^ 0xFF));
                    if (coltab[8] & m)
                        d = d | (coltab[8] & (m ^ 0xFF));
                    if (coltab[0x10] & m)
                        d = d | (coltab[0x10] & (m ^ 0xFF));
                    if (coltab[0x20] & m)
                        d = d | (coltab[0x20] & (m ^ 0xFF));
                    if (coltab[0x80] & m)
                        d = d | (coltab[0x80] & (m ^ 0xFF));
                }
                m = m << 1;
            }
            mcs48::G7400FreezeTrace_NoteA2PreClear();
            if (CPU_IsrTraceActive())
                fprintf(stderr, "ISR_R adr=A2 d=%02X p1=%02X p2=%02X pc=%03X frame=%d mclk=%d\n",
                    unsigned(d), unsigned(p1), unsigned(p2), unsigned(pc), frame, master_clk);
            clear_collision();
            return d;
        case 0xA5:
            if (!(VDCwrite[0xA0] & 0x02)) {
                if (CPU_IsrTraceActive())
                    fprintf(stderr, "ISR_R adr=A5 latched d=%02X\n", unsigned(x_latch));
                return x_latch;
            } else {
                x_latch = VM_UnifiedVideoTestEnabled()
                    ? static_cast<Byte>(VM_UnifiedVideoXBeam())
                    : static_cast<Byte>(h_clk * 12);
                if (CPU_IsrTraceActive())
                    fprintf(stderr, "ISR_R adr=A5 live d=%02X h=%d frame=%d mclk=%d\n",
                        unsigned(x_latch), h_clk, frame, master_clk);
                return x_latch;
            }
        case 0xA4:
            if (!(VDCwrite[0xA0] & 0x02)) {
                if (CPU_IsrTraceActive())
                    fprintf(stderr, "ISR_R adr=A4 latched d=%02X\n", unsigned(y_latch));
                return y_latch;
            } else {
                y_latch = VM_A4BeamValue(master_clk);
                if (CPU_IsrTraceActive())
                    fprintf(stderr, "ISR_R adr=A4 live d=%02X line=%d frame=%d mclk=%d\n",
                        unsigned(y_latch), static_cast<int>(VM_PalA4BeamValue(master_clk)),
                        frame, master_clk);
                return y_latch;
            }
        default:
            // O2EM 1.20 / Videopac+ VDC compatibility.
            // Some VDC addresses are write-only/unused and do not read back
            // the value stored in our software mirror.
            if (adr == 0xA3 || adr == 0xA6 ||
                (adr >= 0xA7 && adr <= 0xA9) ||
                (adr >= 0xAB && adr <= 0xBF) ||
                (adr >= 0xC9 && adr <= 0xCF) ||
                (adr >= 0xD9 && adr <= 0xDF) ||
                (adr >= 0xEA && adr <= 0xFF))
                return static_cast<Byte>(adr);

            // When foreground objects are enabled (A0 bit 5), object x/y/attr
            // registers 00-7F are not accessible and read back as 0. This is
            // the MAME 0.289 / Manopac i8244 semantics (the previous bitwise
            // (adr & 0x02) ? 0x00 : 0xFF form returned 0xFF for even offsets,
            // which is wrong). Sprite shape registers (>=0x80) stay accessible.
            if ((VDCwrite[0xA0] & 0x20) && adr < 0x80)
            {
                const Byte result = 0x00;
                // Observation only: report the value this same read returned.
                if (adr == 0x24) mcs48::G7400FreezeTrace_NoteVdc24Read(result, 1);
                return result;
            }

            // With the grid enabled, reads in the grid register area return 0.
            if ((VDCwrite[0xA0] & 0x08) && adr >= 0xC0 && adr <= 0xE9)
            {
                if (adr == 0x24) mcs48::G7400FreezeTrace_NoteVdc24Read(0x00, 2);
                return 0x00;
            }

            if (adr == 0x24) mcs48::G7400FreezeTrace_NoteVdc24Read(VDCwrite[adr], 0);
            return VDCwrite[adr];
        }
    } else if (!(p1 & 0x10)) {
        /* Handle ext RAM Read */
        if (app_data.megaxrom && (adr >= 0x80)) {
            /* MegaCART registers are mirrored every 4 bytes */
            if ((adr & 0x83) == 0x83) {
                /* TODO: emulate EEPROM data in */
                return 0xff;
            } else
                return extRAM[adr & 0x83];
        } else if (adr < 0x80) {
            return extRAM[adr & 0xFF];
        }
        /* 0x80-0xFF is not external RAM; fall through to the cart/VDC/open-bus
           decode instead of returning extRAM[adr]. */
    } else if (!(p1 & 0x20)) {
        /* Read a Videopac+ register */
        return vpp_read(adr);
    } else if (app_data.exrom && (p1 & 0x02)) {
        /* Handle read from exrom */
        return extROM[((p2 & 0x0F) << 8) | (adr & 0xFF)];
    } else if (app_data.megaxrom && !(p1 & 0x02) && !(p1 & 0x40)) {
        /* Handle data read from MegaCART */
        return megarom[(extRAM[0x81] << 12) | ((p2 & 0x0f) << 8) | (adr & 0xff)];
    }

    // Unclaimed I/O reads return the cartridge bus default (0xFF for a
    // standard cartridge), matching MAME's io_read().
    return 0xFF;
}

static Byte in_bus_observed_impl(void);
Byte in_bus(void)
{
    const Byte value = in_bus_observed_impl();
    mcs48::observation::Event(11, 0, value);
    return value;
}

static Byte in_bus_observed_impl(void)
{
    Byte si = 0, d = 0, mode = 0, jn = 0;

    if ((p1 & 0x08) && (p1 & 0x10)) {
        /* Handle joystick read */
        if (!(p1 & 0x04)) {
            si = (p2 & 7);
        }
        d = 0xFF;
        if (si == 1) {
            mode = app_data.stick[0];
            jn = 0;
        } else {
            mode = app_data.stick[1];
            jn = 1;
        }
        switch (mode) {
        case 1:
        case 2:
            d = keyjoy(jn);
            break;
        }
        if (si == 1) {
            if (dbstick1)
                d = dbstick1;
        } else {
            if (dbstick2)
                d = dbstick2;
        }
    }
    return d;
}

static bool GridTraceEnabled(void)
{
    static int enabled = -1;
    if (enabled < 0)
    {
        const char* env = getenv("O2EM_GRID_TRACE");
        enabled = (env && *env && *env != '0') ? 1 : 0;
    }
    return enabled == 1;
}

void ext_write(Byte dat, ADDRESS adr)
{
    mcs48::observation::Event(3, adr, dat);
    // Patch 0030K: observe the real 8048 MOVX write before C7010/VDC routing.
    C7010_Trace8048ExternalAccess(true, adr, dat);

    // 0030R: cartridge and VDC may both observe the same write.
    const bool c7010Handled = C7010_ExternalWrite(adr, dat);
    if (c7010Handled && (p1 & 0x08))
        return;

    int i;

    if (CPU_IsrTraceActive())
        fprintf(stderr, "ISR_W adr=%02X dat=%02X A0=%02X p1=%02X p2=%02X pc=%03X frame=%d mclk=%d\n",
            unsigned(adr), unsigned(dat), unsigned(VDCwrite[0xA0]),
            unsigned(p1), unsigned(p2), unsigned(pc), frame, master_clk);

    if (!(p1 & 0x08)) {
        /* Handle VDC Write */
        mcs48::observation::Event(9, adr, dat);

        // Development-only VDC write trace for the video investigation.
        static int vdcTraceEnabled = -1;
        if (vdcTraceEnabled < 0)
        {
            const char* env = getenv("O2EM_VDC_WRITE_TRACE");
            vdcTraceEnabled = (env && *env && *env != '0') ? 1 : 0;
        }
        static int objTraceFrame = -1;
        {
            static bool objTraceInit = false;
            if (!objTraceInit)
            {
                objTraceInit = true;
                const char* env = getenv("O2EM_OBJ_TRACE_FRAME");
                if (env && *env)
                    objTraceFrame = static_cast<int>(strtol(env, nullptr, 10));
            }
        }
        if (objTraceFrame >= 0 && frame == objTraceFrame &&
            (adr <= 0x9F || adr == 0xA0 || adr == 0xA3))
        {
            fprintf(stderr,
                "OBJ_W frame=%d clk=%d h=%d adr=%02X old=%02X new=%02X A0=%02X pc=%03X ubl=%d\n",
                frame, master_clk, h_clk, unsigned(adr),
                unsigned(VDCwrite[adr]), unsigned(dat),
                unsigned(VDCwrite[0xA0]), unsigned(pc),
                VM_UnifiedVideoTestEnabled() ? VM_UnifiedVideoBeamLine()
                    : static_cast<int>((static_cast<long long>(master_clk) * 18 + 228) / 456));
        }
        if (vdcTraceEnabled && frame >= 60 && frame <= 100 &&
            (adr == 0xA0 || adr <= 0x07 || adr == 0x1A || adr == 0x1E ||
             (adr >= 0x80 && adr <= 0x9F) || (adr >= 0xC0 && adr <= 0xEF)))
        {
            fprintf(stderr,
                "VDC_W frame=%d clk=%d h=%d last=%d mstate=%d pc=%03X adr=%02X old=%02X new=%02X A0=%02X\n",
                frame, master_clk, h_clk, last_line, mstate, unsigned(pc),
                unsigned(adr), unsigned(VDCwrite[adr]), unsigned(dat),
                unsigned(VDCwrite[0xA0]));
        }

        // Original O2EM/VDC behaviour:
        // while foreground-object display is enabled (A0 bit 5), writes to
        // object RAM 00-7F are ignored by the hardware.
        C7010_TraceVdcWrite(adr, dat, (VDCwrite[0xA0] & 0x20) && adr <= 0x7F, master_clk);
        if ((VDCwrite[0xA0] & 0x20) && adr <= 0x7F)
        {
            // Observation only: the production write is dropped here.
            if (adr == 0x24) mcs48::G7400FreezeTrace_NoteVdc24Write(dat, VDCwrite[0x24], VDCwrite[0x24], true);
            return;
        }

        // i8244 major-systems Y CAM d0 is not connected (MAME 0.289
        // i8244_device::write): the stored/readback value always has bit 0
        // clear for registers >= 0x10 with (offset & 0x83) == 0. Without this
        // mask a write of 0xFF reads back as 0xFF instead of 0xFE.
        if (adr >= 0x10 && (adr & 0x83) == 0x00)
            dat &= static_cast<Byte>(~0x01);

        // Writing grid registers C0-EF while the grid is enabled does not
        // store the CPU byte reliably. O2EM models the observed bus garbage
        // with the current CPU clock value.
        // i8244 semantics (MAME 0.289): a write to the object, shape, control
        // or color registers updates the screen up to the current beam before
        // the new value takes effect. VP48 multiplexes the 12 foreground and
        // 4 quad registers during the frame; rendering only at A0/VBL loses
        // that raster history and shows only the last set of objects.
        const bool mameUpdate = VDCStub_MameUpdateEnabled();
        if (mameUpdate
                ? ((adr <= 0xA0 || adr == 0xA2 || adr == 0xA3) && dat != VDCwrite[adr])
                : (adr != 0xA0 && (adr <= 0xA0 || adr == 0xA3) && dat != VDCwrite[adr]))
        {
            draw_region();
        }
        if ((VDCwrite[0xA0] & 0x08) && adr >= 0xC0 && adr <= 0xEF)
        {
            if (GridTraceEnabled() && frame >= 30)
            {
                fprintf(stderr,
                    "GRID_HACK frame=%d adr=%02X in=%02X old=%02X clk=%d stored=%02X A0=%02X pc=%03X\n",
                    frame, unsigned(adr), unsigned(dat), unsigned(VDCwrite[adr]), clk,
                    unsigned(static_cast<Byte>(clk & 0xFF)),
                    unsigned(VDCwrite[0xA0]), unsigned(pc));
            }
            dat = static_cast<Byte>(clk & 0xFF);
        }
        else if (GridTraceEnabled() && frame >= 30 && adr >= 0xC0 && adr <= 0xEF)
        {
            static bool liveDumped = false;
            if (!liveDumped && pc >= 0x400)
            {
                liveDumped = true;
                const int bank = static_cast<int>(rom - rom_table[0]) / 4096;
                std::fprintf(stderr, "GRID_LIVE bank=%d pc=%03X bytes=", bank, unsigned(pc));
                for (int k = -8; k <= 8; ++k)
                    std::fprintf(stderr, "%02X", unsigned(rom[(pc + k) & 0xFFF]));
                std::fprintf(stderr, "\n");
            }
            fprintf(stderr, "GRID_WRITE frame=%d adr=%02X old=%02X dat=%02X A0=%02X pc=%03X op=%02X\n",
                frame, unsigned(adr), unsigned(VDCwrite[adr]), unsigned(dat),
                unsigned(VDCwrite[0xA0]), unsigned(pc), unsigned(rom[pc - 1]));
        }
        if (adr == 0xA0) {
            const Byte a0Old = VDCwrite[0xA0];
            const Byte yBefore = y_latch;
            if (GridTraceEnabled() && frame >= 30 && a0Old != dat)
            {
                fprintf(stderr, "GRID_A0 frame=%d old=%02X new=%02X clk=%d mclk=%d line=%d pc=%03X op=%02X\n",
                    frame, unsigned(a0Old), unsigned(dat), clk, master_clk,
                    static_cast<int>(VM_PalA4BeamValue(master_clk)),
                    unsigned(pc), unsigned(rom[pc - 1]));
            }
            if ((VDCwrite[0xA0] & 0x02) && !(dat & 0x02)) {
                y_latch = VM_A4BeamValue(master_clk);
                x_latch = h_clk * 12;
            }
            if (!mameUpdate &&
                (master_clk <= VM_EffectiveVblClk()) && (VDCwrite[0xA0] != dat)) {
                draw_region();
            }
            // Observational only: report the write exactly as processed above.
            // Does not perform or duplicate the latch operation.
            mcs48::G7400FreezeTrace_NoteA0Write(a0Old, dat, yBefore, y_latch);
        } else if (adr == 0xA3) {
            int l = VM_UnifiedVideoTestEnabled()
                ? VM_UnifiedVideoBeamLine()
                : snapline(RenderBeamLine(master_clk, 0.5), dat, 1);
            for (i = l; i < MAXLINES; i++)
                ColorVector[i] = (dat & 0x7f) | (p1 & 0x80);
        } else if (adr == 0xAA) {
            audio_vdc_control_write(VDCwrite[0xAA], dat, master_clk / 22);
            for (i = master_clk / 22; i < MAXLINES; i++)
                AudioVector[i] = dat;
        } else if (adr >= 0xA7 && adr <= 0xA9) {
            // Feed writes into the persistent 24-bit Intel 8244 sound
            // register. VDCwrite still retains the CPU-visible bytes.
            audio_vdc_write(adr, dat);
        } else if ((adr >= 0x40) && (adr <= 0x7f) && ((adr & 2) == 0)) {
            /* simulate quad: all 4 sub quad position registers
             * are mapped to the same internal register */
            adr = adr & 0x71;
            /* Another minor thing: the y register always returns
             * bit 0 as 0 */
            if ((adr & 1) == 0)
                dat = dat & 0xfe;
            VDCwrite[adr] = VDCwrite[adr + 4] = VDCwrite[adr + 8] = VDCwrite[adr
                    + 12] = dat;
        }
        // Observation only: report the store exactly as the production path
        // performs it (before = current mirror, after = the value stored).
        if (adr == 0x24) mcs48::G7400FreezeTrace_NoteVdc24Write(dat, VDCwrite[0x24], dat, false);
        VDCwrite[adr] = dat;
    } else if (!(p1 & 0x10) && !(p1 & 0x40)) {
        adr = adr & 0xFF;

        if (adr < 0x80) {
            /* Handle ext RAM Write */
            C7010_TraceBoardWrite(adr, extRAM[adr], dat);
            extRAM[adr] = dat;

        } else {
            if (app_data.bank == 4) {
                romlatch = (~dat) & 7;
                rom = rom_table[(p1 & 1) ? 0 : romlatch];
            }

            /* Handle The Voice */
            if (!(dat & 0x20))
                reset_voice();
            else {
                if (adr == 0xE4)
                    set_voice_bank(0);
                else if ((adr >= 0xE8) && (adr <= 0xEF))
                    set_voice_bank(adr - 0xE7);
                else if (((adr >= 0x80) && (adr <= 0xDF))
                        || ((adr >= 0xF0) && (adr <= 0xFF)))
                    trigger_voice(adr);
            }
        }
    } else if (!(p1 & 0x20)) {
        /* Write to a Videopac+ register */
        vpp_write(dat, adr);
    }
}

static void do_kluges(void)
{
    // Every game must begin from the canonical per-game defaults. These are
    // file-scope values: without this reset, a cartridge-specific kluge
    // (Blockout's enahirq/regionoff, Atlantis' pendirq, Turtles' useforen,
    // ...) leaks into the next game and makes "game A -> game B" differ from
    // "fresh start -> game B".
    pendirq = 0;
    regionoff = 0xffff;
    enahirq = 1;
    useforen = 0;
    sproff = 0;
    mxsnap = 2;
    tweakedaudio = 0;

#if defined(_DEBUG)
    printf(
        "O2EM-NG: KLUDGE defaults: pendirq=%d regionoff=%ld enahirq=%d "
        "useforen=%d sproff=%d mxsnap=%d tweakedaudio=%d\n",
        pendirq, regionoff, enahirq, useforen, sproff, mxsnap, tweakedaudio);
    fflush(stdout);
#endif

    if (app_data.crc == 0xA7344D1F)
        pendirq = 1; /* Atlantis */
    if (app_data.crc == 0xFB83171E)
        pendirq = 1; /* Blockout */
    if (app_data.crc == 0xD38153F6)
        pendirq = 1; /* Blockout (french) */
    if (app_data.crc == 0x881CEAE4)
        pendirq = 1; /* Wall Street */

    if (app_data.crc == 0x9E42E766)
        useforen = 1; /* Turtles */
    if (app_data.crc == 0x1C750349)
        useforen = 1; /* Turtles (European version) */
    if (app_data.crc == 0x202F2749)
        useforen = 1; /* Q*bert */
    if (app_data.crc == 0x06861A9C)
        useforen = 1; /* Flashpoint 5 (Videopac adaption) */

    if (app_data.crc == 0xFB83171E)
        enahirq = 0; /* Blockout*/
    if (app_data.crc == 0xD38153F6)
        enahirq = 0; /* Blockout (french) */

    if (app_data.crc == 0xFB83171E)
        regionoff = 1; /* Blockout*/
    if (app_data.crc == 0xD38153F6)
        regionoff = 1; /* Blockout (french) */
    if (app_data.crc == 0x202F2749)
        regionoff = 0; /* Q*bert */
    if (app_data.crc == 0x5216771A)
        regionoff = 1; /* Popeye */
    if (app_data.crc == 0x0C2E4811)
        regionoff = 11; /* Out of this World! / Helicopter Rescue! */
    if (app_data.crc == 0x67069924)
        regionoff = 11; /* Smithereens! */
    if (app_data.crc == 0x44D1A8A5)
        regionoff = 11; /* Smithereens! (European version) */
    if (app_data.crc == 0x2391C2FB)
        regionoff = 11; /* Smithereens! + */
    if (app_data.crc == 0xBB4AD548)
        regionoff = 11; /* Smithereens! modified 1 */
    if (app_data.crc == 0x25057C11)
        regionoff = 11; /* Smithereens! modified 2 */
    if (app_data.crc == 0xB936BD78)
        regionoff = 12; /* Type & Tell */
    if (app_data.crc == 0xAD8B9AE0)
        regionoff = 2; /* Type & Tell modified 1 */
    if (app_data.crc == 0x5C02BEE6)
        regionoff = 2; /* Type & Tell modified 2 */
    if (app_data.crc == 0xDC30AD3D)
        regionoff = 10; /* Dynasty! */
    if (app_data.crc == 0x7810BAD5)
        regionoff = 8; /* Dynasty! (European) */
    if (app_data.crc == 0xA7344D1F)
        regionoff = 0; /* Atlantis */
    if (app_data.crc == 0xD0BC4EE6)
        regionoff = 12; /* Frogger */
    if (app_data.crc == 0xA57D84F3)
        regionoff = 8; /* Frogger BR */
    if (app_data.crc == 0x825976A9)
        regionoff = 0; /* Mousing Cat 8kb */
    if (app_data.crc == 0xF390BFEC)
        regionoff = 0; /* Mousing Cat 4kb */
    if (app_data.crc == 0x61A350E6)
        regionoff = 0; /* Mousing Cat (french) */
    if (app_data.crc == 0x3BFEF56B)
        regionoff = 1; /* Four in 1 Row! */
    if (app_data.crc == 0x7C747245)
        regionoff = 1; /* Four in 1 Row! modified */
    if (app_data.crc == 0x9B5E9356)
        regionoff = 1; /* Four in 1 Row! (french) */

    if (app_data.crc == 0x6CEBAB74)
        regionoff = 12; /* P.T. Barnum's Acrobats! (European version) */
    if (app_data.crc == 0xE7B26A56)
        regionoff = 12; /* P.T. Barnum's Acrobats! (European version - Extra keys) */

    if (app_data.crc == 0xFB83171E)
        mxsnap = 3; /* Blockout*/
    if (app_data.crc == 0xD38153F6)
        mxsnap = 3; /* Blockout (french) */
    if (app_data.crc == 0xA57E1724)
        mxsnap = 12; /* Catch the Ball / Noughts and Crosses */
    if (app_data.crc == 0xBE4FF48E)
        mxsnap = 12; /* Catch the Ball / Noughts and Crosses modified */
    if (app_data.crc == 0xFD179F6D)
        mxsnap = 3; /* Clay Pigeon! */
    if (app_data.crc == 0x9C9DDDF9)
        mxsnap = 3; /* Verkehr */
    if (app_data.crc == 0x95936B07)
        mxsnap = 3; /* Super Cobra */
    if (app_data.crc == 0x881CEAE4)
        mxsnap = 3; /* Wall Street */
    if (app_data.crc == 0x9E42E766)
        mxsnap = 0; /* Turtles */
    if (app_data.crc == 0x1C750349)
        mxsnap = 0; /* Turtles (European version) */
    if (app_data.crc == 0xD0BC4EE6)
        mxsnap = 3; /* Frogger */
    if (app_data.crc == 0xA57D84F3)
        mxsnap = 3; /* Frogger BR */
    if (app_data.crc == 0x3BFEF56B)
        mxsnap = 6; /* Four in 1 Row! */
    if (app_data.crc == 0x9B5E9356)
        mxsnap = 6; /* Four in 1 Row! (french) */
    if (app_data.crc == 0x7C747245)
        mxsnap = 6; /* Four in 1 Row! modified */

    if (app_data.crc == 0xA7344D1F)
        setvideomode(1); /* Atlantis */
    if (app_data.crc == 0x39E31BF0)
        setvideomode(1); /* Jake */
    if (app_data.crc == 0x92D0177B)
        setvideomode(1); /* Jake (hack) */
    if (app_data.crc == 0x3351FEDA)
        setvideomode(1); /* Power Lords */
    if (app_data.crc == 0x40AE062D)
        setvideomode(1); /* Power Lords (alternate) */
    if (app_data.crc == 0xD158EEBA)
        setvideomode(1); /* Labirinth */
    if (app_data.crc == 0x26B0FF5B)
        setvideomode(1); /* Nightmare */
    if (app_data.crc == 0xDF36683F)
        setvideomode(1); /* Shark Hunter */
    if (app_data.crc == 0xAF307559)
        setvideomode(1); /* Super Bee 8Kb */
    if (app_data.crc == 0x9585D511)
        setvideomode(1); /* Super Bee 4Kb */
    if (app_data.crc == 0x58FA6766)
        setvideomode(1); /* War of the Nerves */
    if (app_data.crc == 0x58FA6766)
        setvideomode(1); /* War of the Nerves */
    if (app_data.crc == 0x39989464)
        setvideomode(1); /* Hockey! / Soccer! */
    if (app_data.crc == 0x3BFEF56B)
        setvideomode(1); /* Four in 1 Row! */
    if (app_data.crc == 0x9B5E9356)
        setvideomode(1); /* Four in 1 Row! (french) */
    if (app_data.crc == 0x7C747245)
        setvideomode(1); /* Four in 1 Row! modified */
    if (app_data.crc == 0x68560DC7)
        setvideomode(1); /* Jopac Moto Crash */
    if (app_data.crc == 0x020FCA15)
        setvideomode(1); /* Jopac Moto Crash modified (non VP+) */
    if (app_data.crc == 0xC4134DF8)
        setvideomode(1); /* Helicopter Rescue + */
    if (app_data.crc == 0x0D2D721D)
        setvideomode(1); /* Trans American Rally + */
    if (app_data.crc == 0x9D72D4E9)
        setvideomode(1); /* Blobbers */
    if (app_data.crc == 0xB2F0F0B4)
        setvideomode(1); /* Le Tresor Englouti + */
    if (app_data.crc == 0x0B2DEB61)
        setvideomode(1); /* Tutankham */
    if (app_data.crc == 0x313547EB)
        setvideomode(1); /* VP53 */
    if (app_data.crc == 0x06861A9C)
        setvideomode(1); /* Flashpoint 5 (Videopac adaption) */
    if (app_data.crc == 0xA57E1724)
        setvideomode(0); /* Catch the Ball / Noughts and Crosses */
    if (app_data.crc == 0xBE4FF48E)
        setvideomode(0); /* Catch the Ball / Noughts and Crosses modified */
    if (app_data.crc == 0xFB83171E)
        setvideomode(0); /* Blockout*/
    if (app_data.crc == 0xD38153F6)
        setvideomode(0); /* Blockout (french) */
    if (app_data.crc == 0x9BFC3E01)
        setvideomode(0); /* Demon Attack */
    if (app_data.crc == 0x50AF9D45)
        setvideomode(0); /* Demon Attack + */
    if (app_data.crc == 0x9884EF36)
        setvideomode(0); /* Demon Attack + modified */
    if (app_data.crc == 0x4A578DFE)
        setvideomode(0); /* Restaurant ntsc */
    if (app_data.crc == 0x863D5E2D)
        setvideomode(0); /* Shark Hunter ntsc */

    if (app_data.crc == 0xD62814A3)
        evblclk = 12000; /* Pick Axe Pete */
    if (app_data.crc == 0xB2FFB353)
        evblclk = 12000; /* Pick Axe Pete + */
    if (app_data.crc == 0x81C20196)
        evblclk = 12000; /* Pick Axe Pete + (modified) */

    if ((app_data.crc == 0xF390BFEC) || (app_data.crc == 0x825976A9)
            || (app_data.crc == 0x61A350E6)) { /* Mousing Cat */
        setvideomode(1);
        evblclk = 7642;
    }

    if (app_data.crc == 0xD0BC4EE6) { /* Frogger */
        setvideomode(1);
        evblclk = 7642;

    }
    if (app_data.crc == 0x26517E77) { /* Commando Noturno */
        setvideomode(1);
        evblclk = 6100;
        regionoff = 12;
    }
    if (app_data.crc == 0xA57E1724) { /* Catch the ball*/
        regionoff = 5;
        sproff = 1;

    }

    if ((app_data.crc == 0x2DCB77F0) || (app_data.crc == 0xF6882734)) { /* Depth Charge / Marksman */
        setvideomode(1);
        evblclk = 8000;
    }
    if (app_data.crc == 0x881CEAE4) { /* Wall Street */
        setvideomode(1);
        evblclk = 6100;
    }
    if (app_data.crc == 0xD0BC4EE6)
        tweakedaudio = 1; /* Frogger */
    if (app_data.crc == 0xA57D84F3)
        tweakedaudio = 1; /* Frogger BR */
    if (app_data.crc == 0x5216771A)
        tweakedaudio = 1; /* Popeye */
    if (app_data.crc == 0xAFB23F89)
        tweakedaudio = 1; /* Musician */
    if (app_data.crc == 0xC4134DF8)
        tweakedaudio = 1; /* Helicopter Rescue + */
    if (app_data.crc == 0x0D2D721D)
        tweakedaudio = 1; /* Trans American Rally + */

    if (app_data.crc == 0xD3B09FEC)
        sproff = 1; /* Volleyball! */
    if (app_data.crc == 0x551E38A2)
        sproff = 1; /* Volleyball! (french) */

#if defined(_DEBUG)
    printf(
        "O2EM-NG: KLUDGE after CRC %08lX: pendirq=%d regionoff=%ld "
        "enahirq=%d useforen=%d sproff=%d mxsnap=%d tweakedaudio=%d\n",
        app_data.crc, pendirq, regionoff, enahirq, useforen, sproff, mxsnap,
        tweakedaudio);
    fflush(stdout);
#endif

}

int snapline(int pos, Byte reg, int t)
{
    const int total_lines = MAXLINES + 2 * MAXSNAP;

    if (pos < 0)
        pos = 0;

    if (pos >= MAXLINES)
        pos = MAXLINES - 1;

    if (t < 0)
        t = 0;

    if (t > 1)
        t = 1;

    for (int i = 0; i < mxsnap; i++) {
        int line1 = pos + MAXSNAP - i;
        int line2 = pos + MAXSNAP + i;

        if (line1 >= 0 && line1 < total_lines) {
            if (snapedlines[line1][reg][t])
                return pos - i;
        }

        if (line2 >= 0 && line2 < total_lines) {
            if (snapedlines[line2][reg][t])
                return pos + i;
        }
    }

    int line = pos + MAXSNAP;

    if (line >= 0 && line < total_lines)
        snapedlines[line][reg][t] = 1;

    return pos;
}

static void setvideomode(int t)
{
    if (t)
    {
        evblclk = EVBLCLK_PAL;
        fps = FPS_PAL;

        printf("O2EM-NG: Active video mode: PAL (%d FPS)\n", fps);
    }
    else
    {
        evblclk = EVBLCLK_NTSC;
        fps = FPS_NTSC;

        printf("O2EM-NG: Active video mode: NTSC (%d FPS)\n", fps);
    }
}

int savestate(char* filename)
{
    FILE *fn;

    fn = fopen(filename, "wb");
    if (fn == NULL) {
        fprintf(stderr, "Error opening state-file %s: %i\n", filename, errno);
        return (errno);
    }

    fwrite(&app_data.crc, sizeof(app_data.crc), 1, fn);
    fwrite(&app_data.bios, sizeof(app_data.bios), 1, fn);

    fwrite(VDCwrite, 256, 1, fn);
    fwrite(extRAM, 256, 1, fn);
    fwrite(intRAM, 64, 1, fn);

    fwrite(&pc, sizeof(pc), 1, fn);
    fwrite(&sp, sizeof(sp), 1, fn);
    fwrite(&bs, sizeof(bs), 1, fn);
    fwrite(&p1, sizeof(p1), 1, fn);
    fwrite(&p2, sizeof(p2), 1, fn);

    fwrite(&ac, sizeof(ac), 1, fn);
    fwrite(&cy, sizeof(cy), 1, fn);
    fwrite(&f0, sizeof(f0), 1, fn);
    fwrite(&A11, sizeof(A11), 1, fn);
    fwrite(&A11ff, sizeof(A11ff), 1, fn);

    fwrite(&timer_on, sizeof(timer_on), 1, fn);
    fwrite(&count_on, sizeof(count_on), 1, fn);
    fwrite(&reg_pnt, sizeof(reg_pnt), 1, fn);

    fwrite(&tirq_en, sizeof(tirq_en), 1, fn);
    fwrite(&xirq_en, sizeof(xirq_en), 1, fn);
    fwrite(&irq_ex, sizeof(irq_ex), 1, fn);
    fwrite(&xirq_pend, sizeof(xirq_pend), 1, fn);
    fwrite(&tirq_pend, sizeof(tirq_pend), 1, fn);

    fclose(fn);
    return (0);

}

int loadstate(char* filename)
{
    FILE *fn;
    int bios;
    unsigned long crc;

    fn = fopen(filename, "rb");
    if (fn == NULL) {
        // no savefile yet
        return (errno);
    }

    sys_fread(&crc, sizeof(crc), 1, fn);
    if (crc != app_data.crc) {
        // wrong cart
        return (199);
    }

    sys_fread(&bios, sizeof(bios), 1, fn);
    if (bios != app_data.bios) // wrong bios
            {
        return (200 + bios);
    }

    sys_fread(VDCwrite, 256, 1, fn);
    sys_fread(extRAM, 256, 1, fn);
    sys_fread(intRAM, 64, 1, fn);

    sys_fread(&pc, sizeof(pc), 1, fn);
    sys_fread(&sp, sizeof(sp), 1, fn);
    sys_fread(&bs, sizeof(bs), 1, fn);
    sys_fread(&p1, sizeof(p1), 1, fn);
    sys_fread(&p2, sizeof(p2), 1, fn);

    sys_fread(&ac, sizeof(ac), 1, fn);
    sys_fread(&cy, sizeof(cy), 1, fn);
    sys_fread(&f0, sizeof(f0), 1, fn);
    sys_fread(&A11, sizeof(A11), 1, fn);
    sys_fread(&A11ff, sizeof(A11ff), 1, fn);

    sys_fread(&timer_on, sizeof(timer_on), 1, fn);
    sys_fread(&count_on, sizeof(count_on), 1, fn);
    sys_fread(&reg_pnt, sizeof(reg_pnt), 1, fn);

    sys_fread(&tirq_en, sizeof(tirq_en), 1, fn);
    sys_fread(&xirq_en, sizeof(xirq_en), 1, fn);
    sys_fread(&irq_ex, sizeof(irq_ex), 1, fn);
    sys_fread(&xirq_pend, sizeof(xirq_pend), 1, fn);
    sys_fread(&tirq_pend, sizeof(tirq_pend), 1, fn);
    fclose(fn);
    return (0);
}

size_t sys_fread(void *ptr, size_t size, size_t nitems, FILE *stream)
{
    return fread(ptr, size, nitems, stream);
}
