0030AC - C7010 character snapshot experiment
Keep a renderer-only copy of registers 00-7F across foreground-disabled row rewrites.
Characters and quads use the copy; CPU reads/writes, sprites, grid and CPU timing are unchanged.
Refresh when foreground is enabled, at frame start, or when the copy is uninitialized.
This is a compatibility experiment, not a verified hardware latch model.
Retains the tested 0030AB JR correction and 0030Y raster scale.
Visually test flicker/header/ranks 4,5,8 and multiple moves; revert this isolated
rendering experiment if it creates stale character trails or worsens clipping.