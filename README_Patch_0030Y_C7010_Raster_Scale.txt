0030Y - C7010 raster scale test
For C7010 only, draw_region converts master_clk using /22 - 5 instead of /20 - 5.
This matches the existing VDC beam latch scale while preserving the drawing origin.
0030X logs show clock 3991 mapped to line 194, beyond objects at Y=176.
The revised mapping is line 176. At clock 5114 it is 227 instead of 250.
This is a targeted compatibility correction, not a cycle-accurate timing overhaul.
Other cartridge mappings and the 0030R/S/X communication/IRQ fixes are retained.
Validate complete lower pieces, ranks 1/2, file labels, E2-E4 and C2-C4-C5.