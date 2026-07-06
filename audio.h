#pragma once

#include <cstdio>

void update_audio(void);
void init_audio(void);
void close_audio(void);
void init_sound_stream(void);
void mute_audio(void);

extern int sound_IRQ;
extern FILE* sndlog;
