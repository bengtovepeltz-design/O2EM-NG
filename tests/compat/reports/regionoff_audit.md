# `regionoff` audit (Checkpoint B)

`regionoff` is the per-title vertical offset applied by `draw_region()` after the
raster line has been calculated. Checkpoint A flagged it as unexplained legacy
debt. Checkpoint B separates its two possible meanings:

- **A. line-rate compensation** — a value that exists only because the old
  renderer used a wrong cycles-per-line scale (`/20`, `/22`, `/24`).
- **B. fixed display-origin offset** — a genuine vertical origin/crop shift of
  the visible field.

Under `O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1` the unified beam replaces the
line-rate calculation, so **A is eliminated by construction**. The table below
records the remaining values as **B, retained provisionally**. They are not yet
proven against MAME or hardware; the evidence column says so explicitly.

## Values in the current production `do_kluges()`

| CRC | Title | `regionoff` | Classification | Evidence |
|---|---|---:|---|---|
| `FB83171E` | Blockout | 1 | fixed origin | not yet measured; retained |
| `D38153F6` | Blockout (French) | 1 | fixed origin | not yet measured; retained |
| `202F2749` | Q*bert | 0 | fixed origin (zero) | no shift; kept for table completeness |
| `5216771A` | Popeye | 1 | fixed origin | not yet measured; retained |
| `0C2E4811` | Out of this World / Helicopter Rescue | 11 | fixed origin | not yet measured; retained |
| `67069924` | Smithereens! | 11 | fixed origin | not yet measured; retained |
| `44D1A8A5` | Smithereens! (European) | 11 | fixed origin | not yet measured; retained |
| `2391C2FB` | Smithereens! + | 11 | fixed origin | not yet measured; retained |
| `BB4AD548` | Smithereens! modified 1 | 11 | fixed origin | not yet measured; retained |
| `25057C11` | Smithereens! modified 2 | 11 | fixed origin | not yet measured; retained |
| `B936BD78` | Type & Tell | 12 | fixed origin | not yet measured; retained |
| `AD8B9AE0` | Type & Tell modified 1 | 2 | fixed origin | not yet measured; retained |
| `5C02BEE6` | Type & Tell modified 2 | 2 | fixed origin | not yet measured; retained |
| `DC30AD3D` | Dynasty! | 10 | fixed origin | not yet measured; retained |
| `7810BAD5` | Dynasty! (European) / Samurai | 8 | fixed origin | not yet measured; retained |
| `A7344D1F` | Atlantis | 0 | fixed origin (zero) | no shift; kept for table completeness |
| `D0BC4EE6` | Frogger | 12 | fixed origin | not yet measured; retained |
| `A57D84F3` | Frogger BR | 8 | fixed origin | not yet measured; retained |
| `825976A9` | Mousing Cat 8 KiB | 0 | fixed origin (zero) | no shift; kept for table completeness |
| `F390BFEC` | Mousing Cat 4 KiB | 0 | fixed origin (zero) | no shift; kept for table completeness |
| `61A350E6` | Mousing Cat (French) | 0 | fixed origin (zero) | no shift; kept for table completeness |
| `3BFEF56B` | Four in 1 Row! | 1 | fixed origin | not yet measured; retained |
| `7C747245` | Four in 1 Row! modified | 1 | fixed origin | not yet measured; retained |
| `9B5E9356` | Four in 1 Row! (French) | 1 | fixed origin | not yet measured; retained |
| `6CEBAB74` | P.T. Barnum's Acrobats! (European) | 12 | fixed origin | not yet measured; retained |
| `E7B26A56` | P.T. Barnum's Acrobats! (European, extra keys) | 12 | fixed origin | not yet measured; retained |

The default `regionoff = 0xffff` means "no offset". In the unified renderer the
offset is added only when it is not `0xffff`.

## A/B evidence in the local inventory

Titles present locally and covered by the matrix:

| Title | Value | Legacy severity | Unified severity | Note |
|---|---:|---|---|---|
| Popeye | 1 | YELLOW | YELLOW | identical metric |
| Atlantis | 0 | GREEN | GREEN | identical metric |
| Mousing Cat | 0 | YELLOW | YELLOW | identical metric |
| Four in 1 Row | 1 | YELLOW | YELLOW | IoU 0.753 → 0.701, content complete |
| Samurai / Dynasty | 8 | ORANGE | ORANGE | identical metric |
| Jumping Acrobats | 12 | YELLOW | YELLOW | identical metric |
| Blockout | 1 | not in local inventory | — | — |
| Q*bert | 0 | not in local inventory | — | — |
| Frogger | 12 | not in local inventory | — | — |

No locally covered `regionoff` title lost content or changed severity. The two
nonzero cases (`Four in 1 Row`, Samurai/Dynasty) keep the same complete board.

## Why it is still not proof

A fixed `+1`, `+8`, `+11` or `+12` can equally be:

1. a real vertical display-origin offset (the O2EM visible window is cropped
   differently from MAME's 360×243 snapshot), or
2. a residual sub-line/line rounding bias from the old renderer, or
3. a title-specific quirk of the i8244 object/grid Y coordinates.

The Checkpoint B matrix can only show that the value still preserves the
observable output. It cannot distinguish (1) from (2). The next step is to
compare each title's first visible/last visible line and the VDC `A0`/`A3`
write lines against MAME, then replace the magic number with either:

- a documented display-origin constant in the unified geometry, or
- a crop/origin rule derived from the i8244 vertical blanking window.

Until then, `regionoff` remains a documented provisional offset, not a
promoted hardware model.
