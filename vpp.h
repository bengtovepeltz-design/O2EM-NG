/*
 * O2EM-NG modification notice (Clarified Artistic License, section 3):
 * This file is part of the O2EM-derived compatibility layer and has been
 * modified or rewritten for O2EM-NG. O2EM-NG modifications:
 * Copyright (c) 2026 Bengt-Ove Peltz. See Docs/COPYRIGHTS.txt.
 */

#pragma once

#include "types.h"

Byte read_PB(Byte p);
void write_PB(Byte p, Byte val);
Byte vpp_read(ADDRESS adr);
void vpp_write(Byte dat, ADDRESS adr);
void init_vpp(void);
// Diagnostic: latch the VPP layer/state at VBL for the frame-latch experiment.
void vpp_latch_frame(void);

// SDL3 bridge used by the O2EM-NG video backend.  The VPP layer is
// composited on top of the normal 8244 framebuffer immediately before upload.
void vpp_compose(Byte* framebuffer, int width, int height);
