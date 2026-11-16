/*
 * O2EM-NG modification notice (Clarified Artistic License, section 3):
 * This file is part of the O2EM-derived compatibility layer and has been
 * modified or rewritten for O2EM-NG. O2EM-NG modifications:
 * Copyright (c) 2026 Bengt-Ove Peltz. See Docs/COPYRIGHTS.txt.
 */

#include <stdint.h>
#ifndef __CRC32_H
#define __CRC32_H

uint32_t crc32_buf(const void *buf, size_t len);
uint32_t crc32_file(const char *filename);

#endif
