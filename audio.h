#pragma once

#include <cstdio>

void update_audio(void);
void init_audio(void);
void close_audio(void);
void init_sound_stream(void);
void mute_audio(void);

// Notify the persistent Intel 8244 sound core when A7-A9 are written.
void audio_vdc_write(unsigned short address, unsigned char value);

// Debug-only notification for tracing AA control changes.
void audio_vdc_control_write(unsigned char old_value, unsigned char new_value, int scanline);

// Generate one frame of audio from emulator timing and AudioVector.
void audio_generate_frame(int frame_rate, int scanline_count);

extern int sound_IRQ;
extern FILE* sndlog;
