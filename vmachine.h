#ifndef VMACHINE_H
#define VMACHINE_H

#include "types.h"

#define LINECNT 21
#define MAXLINES 500
#define MAXSNAP 50

#define VBLCLK 5493
#define EVBLCLK_NTSC 5964
#define EVBLCLK_PAL 7259

#define FPS_NTSC 60
#define FPS_PAL 50

extern Byte dbstick1, dbstick2;
extern int last_line;

extern int evblclk;

// Reference-derived PAL vertical raster timebase (standard G7000/G7400 PAL).
// Off (NTSC / C52 / Jopac / legacy switch) => historical production values.
int VM_PalVerticalTimingActive(void);
int VM_PalVblCycle(void);
int VM_PalFrameBase(void);
int VM_EffectiveVblClk(void);
int VM_EffectiveFrameClk(void);
unsigned int VM_A4BeamValue(int master);
unsigned int VM_PalA4BeamValue(int master);
void VM_RasterTimingFrameBoundary(void);
void VM_ConfigurePalRasterTiming(void);

// Experimental unified CPU/video/raster timebase (Checkpoint B).
// Off unless O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1.
bool VM_UnifiedVideoTestEnabled(void);
void VM_UnifiedVideoConfigure(void);
void VM_UnifiedVideoReset(void);
void VM_UnifiedVideoAdvance(int cycles);
int VM_UnifiedVideoPhasePixels(void);
int VM_UnifiedVideoHPos(void);
int VM_UnifiedVideoVPos(void);
int VM_UnifiedVideoHBlank(void);
int VM_UnifiedVideoVBlank(void);
int VM_UnifiedVideoT1Level(void);
int VM_UnifiedVideoBeamLine(void);
int VM_UnifiedVideoYBeam(void);
int VM_UnifiedVideoXBeam(void);
int VM_UnifiedVideoTakeVblCross(void);
int VM_UnifiedVideoTakeFrameCross(void);

extern int master_clk;		/* Master clock */
extern int int_clk;		/* counter for length of /INT pulses for JNI */
extern int h_clk;   /* horizontal clock */
extern Byte coltab[256];
extern int mstate;

extern Byte rom_table[8][4096];
extern Byte intRAM[];
extern Byte extRAM[];
extern Byte extROM[];
extern Byte VDCwrite[256];
extern Byte ColorVector[MAXLINES];
extern Byte AudioVector[MAXLINES];
extern Byte *rom;
extern Byte *megarom;

extern int frame;
extern int key2[128];
extern int key2vcnt;
extern unsigned long clk_counter;

extern int enahirq;
extern int pendirq;
extern int useforen;
extern long regionoff;
extern int sproff;
extern int tweakedaudio;

Byte read_P2(void);
int snapline(int pos, Byte reg, int t);
void ext_write(Byte dat, ADDRESS adr);
Byte ext_read(ADDRESS adr);
void handle_vbl(void);
void handle_evbl(void);
void handle_evbll(void);
Byte in_bus(void);
void write_p1(Byte d);
Byte read_t1(void);
void init_system(void);
void O2EM_SetRegionModeOverride(int mode);
void O2EM_RequestReset(void);
void init_roms(void);
void run(void);
int savestate(char* filename);
int loadstate(char* filename);
size_t sys_fread(void * ptr, size_t size, size_t nitems, FILE * stream);

extern struct resource {
	int bank;
	int debug;
	int stick[2];
	int sticknumber[2];
	int limit;
	int sound_en;
	int speed;
	int wsize;
	int fullscreen;
	int scanlines;
	int voice;
	int svolume;
	int vvolume;	
	int exrom;
	int three_k;
	int filter;
	int euro;
	int openb;
	int megaxrom;
	int vpp;
	int bios;
	unsigned long crc;
	char *window_title;
	char *scshot;
	int scoretype;
	int scoreaddress;
	int default_highscore;
	int breakpoint;
	char *statefile;
} app_data;


#endif  /* VMACHINE_H */

