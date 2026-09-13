#pragma once

#include "types.h"

Byte read_PB(Byte p);
void write_PB(Byte p, Byte val);
Byte vpp_read(ADDRESS adr);
void vpp_write(Byte dat, ADDRESS adr);
void init_vpp(void);

// SDL3 bridge used by the O2EM-NG video backend.  The VPP layer is
// composited on top of the normal 8244 framebuffer immediately before upload.
void vpp_compose(Byte* framebuffer, int width, int height);
