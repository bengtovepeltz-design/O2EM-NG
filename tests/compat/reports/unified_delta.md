# Checkpoint B: unified video timebase A/B delta

Generated from `tests/compat/matrix/compat_matrix_legacy.csv` and `tests/compat/matrix/compat_matrix_unified.csv`.

Both runs use the same ROMs, BIOS selection, region, scripted input, frame checkpoints (500/800) and the same MAME 0.289 reference captures (`-SkipMame`, captured in Checkpoint A). The only difference is `O2EM_UNIFIED_VIDEO_TIMEBASE_TEST=1`.

Severity is the existing automated geometry triage (lit-mask IoU + row/column profile correlation). It is a triage signal, not a pixel-perfect claim; palette is excluded.

The legacy run reproduces the Checkpoint A baseline row-for-row (85/85 titles have identical severity and `state_alignment` strings), so the production path is unchanged. The single NTSC inventory title (Super Cobra Brazil) ran the unified NTSC geometry (455×263, 20 px/cycle) and kept its YELLOW severity.

## Summary

| Severity | Legacy | Unified |
|---|---:|---:|
| GREEN | 18 | 17 |
| YELLOW | 39 | 40 |
| ORANGE | 9 | 9 |
| RED | 0 | 0 |
| UNKNOWN | 19 | 19 |
| **Total** | **85** | **85** |

| Classification | Titles |
|---|---:|
| IMPROVED | 0 |
| UNCHANGED | 65 |
| REGRESSED | 1 |
| UNKNOWN | 19 |
| **Total** | **85** |

## IMPROVED titles (complete)

_None._

## REGRESSED titles (complete)

| VP | Title | ROM | System | Region | Legacy | Unified | IoU legacy | IoU unified |
|---|---|---|---|---|---|---|---:|---:|
| VP48 | Backgammon | vp_48.bin | G7000 | PAL | GREEN | YELLOW | 0.780 | 0.771 |

## UNKNOWN in at least one run (complete)

| VP | Title | ROM | System | Region | Legacy | Unified | IoU legacy | IoU unified |
|---|---|---|---|---|---|---|---:|---:|
| — |  | Bird Hunt (Ahnl66).bin | G7000 | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — |  | Kill the Attacking Aliens — demo 2.bin | G7000 | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — | written by Rene van den Enden | new_pong.bin | G7000 | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — |  | twobv.bin | G7000 | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP01+ | Race / Spin-Out / Cryptogram (Europe, plus) | vp_01+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP06+ | Ten Pin Bowling / Basketball (Europe, plus, prototype) | vp_06+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP11+ | Cosmic Conflict (Europe, plus) | vp_11+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP20+ | Stone Sling (Europe, plus) | vp_20+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP43+ | Pickaxe Pete (Europe, plus) | vp_43+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP53+ | Phantom House (Europe, prototype) | vp_53+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP54+ | Loony Balloon (Europe, plus) | vp_54+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP55+ | Neutron Star (Europe, prototype) | vp_55+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP56+ | Norseman (Europe) | vp_56+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP58+ | Air Battle (Europe, prototype, set 1) | vp_58+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP59+ | Helicopter Rescue (Europe) | vp_59+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP60+ | Trans American Rally (Europe) | vp_60+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP62+ | Clay Pigeon (Europe, plus, prototype, set 1) | vp_62+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| vp_C7010 | Chess (not complete, game has extra microprocessor module) | vp_C7010.bin | G7000 | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| vp_Tutankham+ | Tutankham (Europe, prototype) | vp_Tutankham+.bin | G7400/VPP | PAL | UNKNOWN | UNKNOWN | 0.000 | 0.000 |

## Numeric movement (IoU)

Severity is coarse; this table shows the underlying geometry metric. A small movement is expected from the exact integer beam (`floor(phase/htotal)`) replacing the legacy rounded line index and from bypassing snapline/mxsnap.

| VP | Title | ROM | Severity | IoU legacy | IoU unified | Delta |
|---|---|---|---|---:|---:|---:|
| VP29 | Dam Buster | vp_29.bin | YELLOW → YELLOW | 0.629 | 0.633 | +0.004 |
| VP44 | Crazy Chase | vp_44.bin | YELLOW → YELLOW | 0.431 | 0.432 | +0.001 |
| VP43 | Pick Axe Pete | vp_43.bin | YELLOW → YELLOW | 0.545 | 0.546 | +0.001 |
| — | Super Cobra (Brazil) | Super Cobra (1983)(Parker Brothers)(BR).bin | YELLOW → YELLOW | 0.407 | 0.408 | +0.001 |
| VP64 | Shark Hunter (Europe, prototype) | vp_64.bin | GREEN → GREEN | 0.857 | 0.857 | +0.000 |
| VP61 | Interpol (Europe, prototype) | vp_61.bin | GREEN → GREEN | 0.650 | 0.650 | +0.000 |
| VP60 | Trans American Rally + (works only on G7400 and Jopac) | vp_60_16.bin | ORANGE → ORANGE | 0.375 | 0.375 | +0.000 |
| VP60 | Trans American Rally (Europe) | vp_60.bin | ORANGE → ORANGE | 0.375 | 0.375 | +0.000 |
| VP46 | The Great Wall Street Fortune Hunt | vp_46.bin | YELLOW → YELLOW | 0.800 | 0.730 | -0.070 |
| VP52 | Killer Bees (Europe, plus) | vp_52.bin | YELLOW → YELLOW | 0.442 | 0.373 | -0.069 |
| VP40 | 4-in-1 Row | vp_40.bin | YELLOW → YELLOW | 0.753 | 0.701 | -0.052 |
| VP65 | Spider-Man (Europe, prototype, set 1) | vp_65.bin | GREEN → GREEN | 0.902 | 0.852 | -0.050 |
| VP51 | Terrahawks (Europe, plus) | vp_51.bin | YELLOW → YELLOW | 0.585 | 0.536 | -0.049 |
| VP49 | Turtles | vp_49.bin | YELLOW → YELLOW | 0.488 | 0.442 | -0.046 |
| VP48 | Backgammon | vp_48.bin | GREEN → YELLOW | 0.780 | 0.771 | -0.009 |
| — | AMOK! | Amok (1998)(Dondzilla, John).bin | GREEN → GREEN | 0.615 | 0.615 | +0.000 |

## Mandatory probe titles

| Probe | ROM | Legacy | Unified | IoU legacy | IoU unified |
|---|---|---|---|---:|---:|
| VP48 | vp_48.bin | GREEN | YELLOW | 0.780 | 0.771 |
| VP60 | vp_60.bin | ORANGE | ORANGE | 0.375 | 0.375 |
| VP61 | vp_61.bin | GREEN | GREEN | 0.650 | 0.650 |
| vp_C7010 | vp_C7010.bin | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP15 | vp_15.bin | ORANGE | ORANGE | 0.185 | 0.185 |
| VP16 | vp_16.bin | ORANGE | ORANGE | 0.080 | 0.080 |
| VP20 | vp_20.bin | YELLOW | YELLOW | 0.788 | 0.788 |
| VP40 | vp_40.bin | YELLOW | YELLOW | 0.753 | 0.701 |
| VP59+ | vp_59+.bin | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP01 | vp_01.bin | YELLOW | YELLOW | 0.469 | 0.469 |
| VP43 | vp_43.bin | YELLOW | YELLOW | 0.545 | 0.546 |
| Popeye (Europe) | Popeye (1983)(Parker Brothers)(EU).bin | YELLOW | YELLOW | 0.750 | 0.750 |
| VP11 | vp_11.bin | GREEN | GREEN | 0.820 | 0.820 |
| VP04 | vp_04.bin | GREEN | GREEN | 0.913 | 0.913 |
| Atlantis (Europe, USA) | Atlantis (1983)(Imagic)(EU-US).bin | GREEN | GREEN | 0.945 | 0.945 |
| VP19 | vp_19.bin | ORANGE | ORANGE | 0.193 | 0.193 |
| VP47 | vp_47.bin | YELLOW | YELLOW | 0.478 | 0.478 |
| VP52 | vp_52.bin | YELLOW | YELLOW | 0.442 | 0.373 |
| Frogger | _not in local inventory_ | — | — | — | — |
| Blockout | _not in local inventory_ | — | — | — | — |
| Q*bert | _not in local inventory_ | — | — | — | — |
| Commando Noturno | _not in local inventory_ | — | — | — | — |

Three-panel montages are in `tests/compat/reports/probes/` (legacy | unified | MAME 0.289 where available). `vp60_trans_american_rally.png` and `c7010_chess.png` use the custom G7400/chess input sequences because the automated 500/800 checkpoints do not reach their meaningful screens.

## All titles

| VP | Title | ROM | Legacy | Unified | Class | IoU legacy | IoU unified |
|---|---|---|---|---|---|---:|---:|
| — | AMOK! | Amok (1998)(Dondzilla, John).bin | GREEN | GREEN | UNCHANGED | 0.615 | 0.615 |
| — | Atlantis (Europe, USA) | Atlantis (1983)(Imagic)(EU-US).bin | GREEN | GREEN | UNCHANGED | 0.945 | 0.945 |
| — |  | Bird Hunt (Ahnl66).bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — | Exojet+ (France) | Exojet (198x)(Philips)(FR).bin | YELLOW | YELLOW | UNCHANGED | 0.571 | 0.571 |
| — |  | Kill the Attacking Aliens — demo 2.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — | Moto-Crash+ (France) | Moto-Crash (France).bin | ORANGE | ORANGE | UNCHANGED | 0.000 | 0.000 |
| — | Popeye (Europe) | Popeye (1983)(Parker Brothers)(EU).bin | YELLOW | YELLOW | UNCHANGED | 0.750 | 0.750 |
| — | Super Cobra (Brazil) | Super Cobra (1983)(Parker Brothers)(BR).bin | YELLOW | YELLOW | UNCHANGED | 0.407 | 0.408 |
| — | written by Ted (Foolery) Szczypiorski | new_planet-lander.bin | GREEN | GREEN | UNCHANGED | 0.586 | 0.586 |
| — | written by Rene van den Enden | new_pong.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| — |  | twobv.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP01+ | Race / Spin-Out / Cryptogram (Europe, plus) | vp_01+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP01 | Race/Spin-out/Cryptogram | vp_01.bin | YELLOW | YELLOW | UNCHANGED | 0.469 | 0.469 |
| VP02 | Pairs/Space Rendezvous/Logic | vp_02.bin | GREEN | GREEN | UNCHANGED | 0.813 | 0.813 |
| VP03 | American Football | vp_03.bin | YELLOW | YELLOW | UNCHANGED | 0.432 | 0.432 |
| VP04 | Air-Sea War/Battle | vp_04.bin | GREEN | GREEN | UNCHANGED | 0.913 | 0.913 |
| VP05 | Blackjack | vp_05.bin | GREEN | GREEN | UNCHANGED | 0.583 | 0.583 |
| VP06+ | Ten Pin Bowling / Basketball (Europe, plus, prototype) | vp_06+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP06 | Tenpin Bowling/Basketball | vp_06.bin | GREEN | GREEN | UNCHANGED | 0.580 | 0.580 |
| VP07 | Mathematician/Echo | vp_07.bin | ORANGE | ORANGE | UNCHANGED | 0.108 | 0.108 |
| VP08 | Baseball | vp_08.bin | YELLOW | YELLOW | UNCHANGED | 0.416 | 0.416 |
| VP09 | Computer Programmer | vp_09.bin | YELLOW | YELLOW | UNCHANGED | 0.298 | 0.298 |
| VP10 | Golf | vp_10.bin | GREEN | GREEN | UNCHANGED | 0.846 | 0.846 |
| VP11+ | Cosmic Conflict (Europe, plus) | vp_11+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP11 | Cosmic Conflict | vp_11.bin | GREEN | GREEN | UNCHANGED | 0.820 | 0.820 |
| VP12 | Take the Money and Run | vp_12.bin | YELLOW | YELLOW | UNCHANGED | 0.430 | 0.430 |
| VP13 | Playschool Maths | vp_13.bin | YELLOW | YELLOW | UNCHANGED | 0.432 | 0.432 |
| VP14 | Gunfighter | vp_14.bin | GREEN | GREEN | UNCHANGED | 0.741 | 0.741 |
| VP15 | Samurai | vp_15.bin | ORANGE | ORANGE | UNCHANGED | 0.185 | 0.185 |
| VP16 | Depth Charge/Marksman | vp_16.bin | ORANGE | ORANGE | UNCHANGED | 0.080 | 0.080 |
| VP17 | Chinese Logic | vp_17.bin | YELLOW | YELLOW | UNCHANGED | 0.455 | 0.455 |
| VP18 | Laser War | vp_18.bin | YELLOW | YELLOW | UNCHANGED | 0.509 | 0.509 |
| VP19 | Catch the Ball/Noughts and Crosses | vp_19.bin | ORANGE | ORANGE | UNCHANGED | 0.193 | 0.193 |
| VP20+ | Stone Sling (Europe, plus) | vp_20+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP20 | Stone Sling | vp_20.bin | YELLOW | YELLOW | UNCHANGED | 0.788 | 0.788 |
| VP21 | Secret of the Pharaohs | vp_21.bin | YELLOW | YELLOW | UNCHANGED | 0.518 | 0.518 |
| VP22 | Space Monster | vp_22.bin | YELLOW | YELLOW | UNCHANGED | 0.420 | 0.420 |
| VP23 | Las Vegas Gambling | vp_23.bin | YELLOW | YELLOW | UNCHANGED | 0.555 | 0.555 |
| VP24 | Flipper Game | vp_24.bin | YELLOW | YELLOW | UNCHANGED | 0.470 | 0.470 |
| VP25 | Skiing | vp_25.bin | GREEN | GREEN | UNCHANGED | 0.787 | 0.787 |
| VP26 | Basket Game | vp_26.bin | YELLOW | YELLOW | UNCHANGED | 0.509 | 0.509 |
| VP27 | Electronic Table Football | vp_27.bin | YELLOW | YELLOW | UNCHANGED | 0.391 | 0.391 |
| VP28 | Electronic Volleyball | vp_28.bin | YELLOW | YELLOW | UNCHANGED | 0.395 | 0.395 |
| VP29 | Dam Buster | vp_29.bin | YELLOW | YELLOW | UNCHANGED | 0.629 | 0.633 |
| VP30 | Battlefield | vp_30.bin | YELLOW | YELLOW | UNCHANGED | 0.428 | 0.428 |
| VP31 | Musician | vp_31.bin | ORANGE | ORANGE | UNCHANGED | 0.000 | 0.000 |
| VP32 | A Labyrinth Game/Supermind | vp_32.bin | YELLOW | YELLOW | UNCHANGED | 0.396 | 0.396 |
| VP33 | Jumping Acrobats | vp_33.bin | YELLOW | YELLOW | UNCHANGED | 0.380 | 0.380 |
| VP34 | Satellite Attack | vp_34.bin | YELLOW | YELLOW | UNCHANGED | 0.476 | 0.476 |
| VP35 | Electronic Billiards | vp_35.bin | GREEN | GREEN | UNCHANGED | 0.875 | 0.875 |
| VP36 | Electronic Soccer/Electronic Ice Hockey | vp_36.bin | GREEN | GREEN | UNCHANGED | 0.779 | 0.779 |
| VP37 | Monkeyshines | vp_37.bin | YELLOW | YELLOW | UNCHANGED | 0.380 | 0.380 |
| VP38 | Munchkin | vp_38.bin | YELLOW | YELLOW | UNCHANGED | 0.449 | 0.449 |
| VP39 | Freedom Fighters | vp_39.bin | YELLOW | YELLOW | UNCHANGED | 0.488 | 0.488 |
| VP40 | 4-in-1 Row | vp_40.bin | YELLOW | YELLOW | UNCHANGED | 0.753 | 0.701 |
| VP41 | Conquest of the World | vp_41.bin | YELLOW | YELLOW | UNCHANGED | 0.632 | 0.632 |
| VP42 | Quest for the Rings | vp_42.bin | YELLOW | YELLOW | UNCHANGED | 0.436 | 0.436 |
| VP43+ | Pickaxe Pete (Europe, plus) | vp_43+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP43 | Pick Axe Pete | vp_43.bin | YELLOW | YELLOW | UNCHANGED | 0.545 | 0.546 |
| VP44 | Crazy Chase | vp_44.bin | YELLOW | YELLOW | UNCHANGED | 0.431 | 0.432 |
| VP45 | Morse | vp_45.bin | GREEN | GREEN | UNCHANGED | 0.665 | 0.665 |
| VP46 | The Great Wall Street Fortune Hunt | vp_46.bin | YELLOW | YELLOW | UNCHANGED | 0.800 | 0.730 |
| VP47 | The Mousing Cat | vp_47.bin | YELLOW | YELLOW | UNCHANGED | 0.478 | 0.478 |
| VP48 | Backgammon | vp_48.bin | GREEN | YELLOW | REGRESSED | 0.780 | 0.771 |
| VP49 | Turtles | vp_49.bin | YELLOW | YELLOW | UNCHANGED | 0.488 | 0.442 |
| VP50 | Super Bee | vp_50.bin | YELLOW | YELLOW | UNCHANGED | 0.446 | 0.446 |
| VP51 | Terrahawks (Europe, plus) | vp_51.bin | YELLOW | YELLOW | UNCHANGED | 0.585 | 0.536 |
| VP52 | Killer Bees (Europe, plus) | vp_52.bin | YELLOW | YELLOW | UNCHANGED | 0.442 | 0.373 |
| VP53+ | Phantom House (Europe, prototype) | vp_53+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP54+ | Loony Balloon (Europe, plus) | vp_54+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP54 | Loony Balloon (no plus graphics, unreleased) | vp_54.bin | YELLOW | YELLOW | UNCHANGED | 0.473 | 0.473 |
| VP55+ | Neutron Star (Europe, prototype) | vp_55+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP56+ | Norseman (Europe) | vp_56+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP57 | Blobbers | vp_57.bin | ORANGE | ORANGE | UNCHANGED | 0.017 | 0.017 |
| VP58+ | Air Battle (Europe, prototype, set 1) | vp_58+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP59+ | Helicopter Rescue (Europe) | vp_59+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP60+ | Trans American Rally (Europe) | vp_60+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP60 | Trans American Rally (Europe) | vp_60.bin | ORANGE | ORANGE | UNCHANGED | 0.375 | 0.375 |
| VP60 | Trans American Rally + (works only on G7400 and Jopac) | vp_60_16.bin | ORANGE | ORANGE | UNCHANGED | 0.375 | 0.375 |
| VP61 | Interpol (Europe, prototype) | vp_61.bin | GREEN | GREEN | UNCHANGED | 0.650 | 0.650 |
| VP62+ | Clay Pigeon (Europe, plus, prototype, set 1) | vp_62+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| VP64 | Shark Hunter (Europe, prototype) | vp_64.bin | GREEN | GREEN | UNCHANGED | 0.857 | 0.857 |
| VP65 | Spider-Man (Europe, prototype, set 1) | vp_65.bin | GREEN | GREEN | UNCHANGED | 0.902 | 0.852 |
| vp_C7010 | Chess (not complete, game has extra microprocessor module) | vp_C7010.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |
| vp_Tutankham+ | Tutankham (Europe, prototype) | vp_Tutankham+.bin | UNKNOWN | UNKNOWN | UNKNOWN | 0.000 | 0.000 |

## Notes

- The first unified run exposed a frame-boundary defect in the experimental mapper: the phase-wrap flag was set before VBL (the initial phase starts near line 312), then consumed when VBL set `mstate=1`. That locked `handle_evbl()` to line 242 and caused eight severity regressions in the first run (Atlantis, Air-Sea War, Gunfighter, Stone Sling, Jumping Acrobats, Killer Bees, Spider-Man, plus the borderline VP48). The unified VBL handler now discards a wrap observed before VBL, so the CPU frame boundary is the post-VBL phase wrap at line 0. Seven of the eight recovered; VP48 remains a sub-threshold metric-only movement (see its probe note). The tables above are from the corrected run.
- ORANGE/UNKNOWN can reflect game state or input timing rather than a renderer defect. Inspect `tests/compat/diff/legacy/<rom>_side.png` and `tests/compat/diff/unified/<rom>_side.png` before promoting an entry to a core defect.
- Titles classified UNKNOWN in both runs are unchanged by the experiment (mostly G7400/VPP references missing locally or unmatched homebrew). The local MAME install lacks the G7400/VPP BIOS and the EF9340 charset, so all `+` titles and VP59+ have no MAME reference.
- VP60 (Trans American Rally) is a Videopac+ title. With the G7400 BIOS it leaves SELECT GAME and reaches the US map/car scene in both legacy and unified runs. Pressing `0` then falls to a near-black stage; the EF9340 background/road/HUD is not rendered. The unified timebase does not change this. This is a VPP/EF9340 rendering gap, not a timebase gap.
- VP48's GREEN→YELLOW movement is metric-only: the board, pieces, dice, score and side panels are complete and the unified image actually matches MAME's score-panel state more closely than legacy. The IoU drop (0.780→0.771) comes from a sub-one-line placement change (exact floor beam vs the legacy rounded line index, with snapline/mxsnap bypassed).
- `regionoff` is retained under unified only as a fixed display-origin offset. Line-rate compensation (`/20`, `/22`, `/24`), C7010 raster scaling, CRC-specific raster branches, `palRenderOffset` and snapline/mxsnap are bypassed under the experiment.
