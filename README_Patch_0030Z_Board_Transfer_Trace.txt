0030Z - board transfer diagnostic
Retains 0030Y raster correction and all existing execution behavior.
At ENTER records the 128-byte external RAM before the move.
For the existing 300-frame trace window records changed external RAM bytes,
including old/new value, address and 8048 writer PC, capped at 1024 events.
Purpose: locate the incorrect source-square update after accepted E2-E4.
No board state, coordinates or timing is patched by this diagnostic.
Test: reset, 1 YES 1, E2-E4, wait for computer response. Then inspect log.