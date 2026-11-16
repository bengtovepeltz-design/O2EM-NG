# Source Provenance

Permanent component-level provenance register for O2EM-NG.

This document records where each major component came from, which licence
applies, which upstream revisions were used, what O2EM-NG added or changed, and
which questions remain open. It is a technical transparency record, not a legal
opinion.

Related documents:

- `Docs/CREDITS.md` — canonical credits and acknowledgements.
- `Docs/THIRD_PARTY_NOTICES.md` — third-party notice mapping.
- `Docs/COPYRIGHTS.txt` — Clarified Artistic License and original O2EM notice.
- `Docs/Licenses/` — full third-party licence texts.
- `Docs/Development/NG-0_Provenance.md` — development-era register with
  additional per-phase detail.

---

## 1. Classification key

| Code | Meaning |
|---|---|
| **A** | Directly reused source or data |
| **B** | Adapted or translated source |
| **C** | Independently implemented hardware behaviour |
| **D** | Information derived from technical documentation |
| **E** | Common programming patterns or hardware requirements |
| **F** | Uncertain or insufficiently documented provenance |

Low source similarity is not, by itself, proof of independent development, and a
shared hardware constant is not, by itself, proof of copied source code. The
classifications below use the audit's code comparisons plus the project's
development history.

---

## 2. Component overview

| Component | Main files | Origin | Licence | Class | O2EM-NG-specific work | Open questions |
|---|---|---|---|---|---|---|
| Original O2EM heritage | `cpu.cpp`, `vmachine.cpp`, `vdc_stub.cpp`, `vpp_sdl.cpp`, `audio_sdl.cpp`, `cset.cpp`, `vpp_cset.cpp`, `crc32.cpp`, legacy headers | Original O2EM by Daniel Boris, Andre de la Rocha and Arlindo M. de Oliveira | Clarified Artistic License | A/B | SDL3 and Windows x64 port, frontend integration, bug fixes, NG integration, C7010 support | Per-file modification notices were historically incomplete; corrected in v5.0.2 |
| MCS48-NG CPU | `src/core_ng/cpu/mcs48_ng.cpp`, `mcs48_ng.h`, `mcs48_state.h`, `mcs48_bus.h` | O2EM-NG, informed by Intel MCS-48 documentation and MAME 0.289 `mcs48.cpp` as a behavioural reference | Clarified Artistic License (project code) | C/D | Complete owned-state 8048 implementation, conformance suite | None material. Token similarity to MAME 0.289 `mcs48.cpp`: 0.066; longest common block 10 tokens (generic bit tests). |
| NG machine, scheduler, timebase | `src/core_ng/machine.cpp`, `scheduler.cpp`, `timebase.h`, `machine.h` | O2EM-NG, with behavioural reference to MAME `odyssey2.cpp` and `screen.cpp` | Clarified Artistic License | C/D | Deterministic scheduler, timebase, machine composition, NG host boundary | MAME is a behavioural reference only; no long verbatim blocks identified. |
| NG memory, bus and cartridge mappers | `src/core_ng/memory/bus.cpp`, `cartridge.cpp`, `mapper.cpp` | O2EM-NG, informed by MAME cartridge device behaviour and published CRC metadata | Clarified Artistic License | C/D/E | Standard, 3K, Rally, 4-in-1 and B-pin handling; project-owned mapper classes | Behavioural reference, not a source copy, on the evidence reviewed. |
| i8244/i8245 VDC | `src/core_ng/video/vdc.cpp`, `vdc.h`, `vdc_state.h`, `i8244.cpp`, `i8245.cpp`, `character_rom.cpp` | Mixed: O2EM-NG renderer and machine integration; register mirror/read/write logic adapted from MAME 0.289 `i8244.cpp`; palette aligned to MAME; CGROM inherited from O2EM `cset.c` and identical to MAME's dump | Clarified Artistic License for O2EM-NG code; BSD-3-Clause for adapted MAME portions | B/A/C | Tick-driven renderer, collision/priority maps, framebuffer, snapshot, audio forwarding, machine integration | MAME adaptation boundary is documented; the CGROM and palette have a dual O2EM/MAME provenance path. Underlying silicon-data copyright remains a legal question. |
| EF9340/EF9341 VPP | `src/core_ng/video/vpp.cpp`, `vpp.h`, `ef9340_charset.cpp` | Mixed: O2EM-NG device state and composition; scanline character drawing adapted from MAME 0.289 `ef9340_1.cpp`; 2,560-byte charset inherited from O2EM `vpp_cset.cpp` | Clarified Artistic License for O2EM-NG code; BSD-3-Clause for adapted MAME portions | B/A/C | Owned VPP state, command protocol, A/B RAM, ExtRAM, composition, snapshot | MAME adaptation boundary is documented. Charset differs from MAME's `BAD_DUMP` image and comes from O2EM. |
| NG audio device | `src/core_ng/audio/audio_device.cpp`, `audio_device.h` | O2EM-NG, informed by Intel/Philips documentation and MAME `i8244.cpp` sound behaviour | Clarified Artistic License | C/D | Owned shift register, HBLANK prescaler, noise, volume/duty, timestamped writes, canonical PCM, snapshot | Token similarity to MAME `i8244.cpp`: 0.070; longest common block 9 tokens. |
| C7010 Chess Module and NSC800 | `c7010.cpp`, `c7010.h`, `c7010_special.cpp`, `c7010_special.h` | O2EM-NG, first added in commit `8410e02` (v0.31.0-beta); no C7010 implementation exists in original O2EM 1.18 | Clarified Artistic License | C | NSC800 interpreter, C7010 RAM/latches, cartridge detection, save states, NG integration | No third-party C7010 ancestry found in the local O2EM tree. If an earlier external source exists, it should be documented. |
| Windows frontend | `src/frontend/*`, `frontend_*.cpp`, `theme_win95.cpp`, `ui_font.cpp`, `input_sdl.cpp`, etc. | O2EM-NG | Clarified Artistic License | C | Windows 95-inspired UI, navigation, settings, library, import, media viewer, controller support | Uses SDL3, SDL3_image, SDL3_ttf, PDFium and the Microsoft runtime (separate licences). |
| Game Library and catalogue | `src/library/*`, `src/database/*`, `Gamedata/o2em-ng.db`, `Gamedata/videopac.xml` | O2EM-NG; catalogue metadata compiled and curated by the project | Clarified Artistic License for project code; catalogue data as distributed with the project | C | Library, metadata, ratings, favorites, play history, safe migration, import centre | Game ROMs, BIOS files, manuals and commercial artwork are not distributed. |
| My Real Collection | `src/collection/*`, collection UI | O2EM-NG | Clarified Artistic License | C | Separate physical-collection database, conditions, wanted list, CSV export | None material. |
| Embedded i8244 CGROM | `src/core_ng/video/character_rom.cpp` | Original O2EM `cset.c` (identical 512 bytes, CRC32 `B46A3F31`); also identical to MAME 0.289 `charset_i8244.bin` | Clarified Artistic License (O2EM path); BSD-3-Clause (MAME path) | A | Relocated into the NG device without value changes | Copyright in the original chip ROM data is a separate legal question; see section 4. |
| Embedded EF9340 charset | `src/core_ng/video/ef9340_charset.cpp` | Original O2EM `vpp_cset.cpp` | Clarified Artistic License | A | Relocated into the NG device without value changes | None material for project licensing. MAME's datasheet-derived image is not used. |
| i8244 palette | `src/core_ng/video/vdc.cpp` | MAME 0.289 `i8244_palette` ordering; same colour values in O2EM `vdc.c` `colortable[1]` | BSD-3-Clause (MAME ordering); Clarified Artistic (O2EM values) | A/E | Used as the canonical pre-RF palette | Colour values are hardware definitions; presentation palette work remains deferred. |
| SDL3, SDL3_image, SDL3_ttf | distributed DLLs; `ui_font.cpp` | libsdl-org; SDL3_ttf statically includes FreeType and HarfBuzz | zlib; FTL; Old MIT | A | Unmodified binaries | Notices included. SDL3_image optional codec DLLs are not distributed. |
| PDFium | `third_party/pdfium`, distributed DLL | PDFium Authors | MIT and third-party notices | A | Unmodified binary | Notice included. |
| Microsoft Visual C++ runtime | `msvcp140.dll`, `vcruntime140.dll`, `vcruntime140_1.dll` | Microsoft | Microsoft Visual Studio license terms | A | Unmodified redistributable binaries | Notice file added; authoritative terms remain Microsoft's. |
| SQLite | Windows `winsqlite3.dll` | Microsoft Windows / SQLite (public domain) | System component / public domain | D | Dynamic load only | Not redistributed. |
| Gameplay screenshots and videos | `SCREENSHOTS` (user media folder) | Captured from emulated games by the project owner; **not bundled in v5.0.2 release packages** | User-supplied media; website images are separate promotional material | F | Package ships an empty `SCREENSHOTS` folder with a README | Website imagery remains subject to the respective game rights holders; see section 4.4. |

---

## 3. MAME adaptation detail

### 3.1 Intel 8244/8245 VDC

The O2EM-NG VDC is not a wholesale copy of MAME. It is a class-based, tick-driven
renderer with its own scheduler, framebuffer and snapshot model.

The audit identified a **near-verbatim adaptation** of MAME's
`i8244_device::fix_register_mirrors` and matching register read/write masks:

- O2EM-NG `src/core_ng/video/vdc.cpp` lines 210–216.
- MAME 0.289 `src/devices/video/i8244.cpp` lines 219–234.

It also identified the same register-mask conditions (`0x83`, `0xD0`,
`0x02/0x06/0x0A/0x0E`, `0x03/0x07/0x0B/0x0F`) and the same 16-colour palette
values. The rest of the renderer is restructured.

Classification: **B** for the adapted logic, **A** for the palette data, **C/E**
for the rewritten renderer.

### 3.2 Thomson EF9340/EF9341 VPP

The O2EM-NG VPP owns its state and composition, but the scanline character
drawing follows MAME's `ef9340_1_device::draw_scanline` closely.

The audit found a **52-token identical block**:

- O2EM-NG `src/core_ng/video/vpp.cpp` lines 258–263.
- MAME 0.289 `src/devices/video/ef9340_1.cpp` lines 317–323.

The shared logic continues through `vpos`, `zoom`, `slice`, `dh`, `char_data`,
`underline`, `w_parity`, `blink`, `cursor`, `invert`, `dw`, `del` and the
delimiter/box handling. This is adaptation/translation, not an independent
re-expression.

Classification: **B** for the drawing logic.

### 3.3 Not copied from MAME

The following components were reviewed and are not MAME source copies on the
evidence available:

- MCS48-NG CPU: token similarity 0.066; longest common block 10 tokens.
- Audio device: token similarity to MAME `i8244.cpp` 0.070; longest common block
  9 tokens.
- C7010/NSC800: token similarity to MAME `z80.cpp` 0.059 and to
  `nsc800.cpp` 0.013.
- Machine, bus and mappers: behavioural reference only; no long verbatim blocks
  identified.

These similarity figures are screening evidence, not legal proof. The project's
development history and the audit's source inspection support independent
implementation for these components.

---

## 4. Embedded data investigation

### 4.1 i8244/i8245 character generator ROM

- Size: 512 bytes.
- CRC32: `B46A3F31`.
- SHA1: `415382715455b47b69401b3d60bd8f0036dd7fef`.
- O2EM-NG location: `src/core_ng/video/character_rom.cpp`.
- Original O2EM location: `cset.c` (`Byte cset[512]`), with the original O2EM
  copyright header. The bytes are identical and produce the same CRC32.
- MAME location: `src/devices/video/i8244.cpp` ROM region `cgrom`,
  `charset_i8244.bin`, same CRC32 and SHA1.

Finding: the data has a documented redistribution path through original O2EM
under the Clarified Artistic License, and it is also present in MAME under
BSD-3-Clause. The O2EM-NG file header previously described only the MAME path;
v5.0.2 records both.

Remaining legal question: whether the original silicon character data is
copyrightable at all, and whether either upstream licence can validly grant
rights in it. This is **not** a confirmed violation, and the data is not removed
without owner approval. It remains a point for qualified legal review.

### 4.2 EF9340/EF9341 character generator data

- Size: 2,560 bytes (256 characters × 10 scanlines).
- CRC32: `D557A7BF`.
- O2EM-NG location: `src/core_ng/video/ef9340_charset.cpp`.
- Origin: original O2EM `vpp_cset.cpp` (`Byte vpp_cset[2][1280]`), Clarified
  Artistic License.
- MAME's `charset_ef9340_1.rom` expects CRC `8DE85988` and is marked `BAD_DUMP`
  (datasheet-derived). O2EM-NG does not use MAME's image.

Finding: documented O2EM provenance and licence path. No MAME data notice is
required for this table beyond the general third-party mapping.

### 4.3 i8244 palette

- O2EM-NG uses the 16 RGB values in MAME's `i8244_palette` order.
- Original O2EM `vdc.c` contains the same colour values in its G7400
  `colortable[1]`, with a different index order.
- The values are hardware colour definitions. The MAME ordering is covered by
  the BSD-3-Clause notice.

### 4.4 Screenshots, videos and artwork

**Release packages do not bundle gameplay screenshots or videos.** The v5.0.2
portable package and installer create an empty `SCREENSHOTS` folder with a
README; users supply their own media. This removes the package-level
redistribution question for captured game visuals.

The official website uses separate, owner-approved promotional screenshots and
videos. They are used for preservation and documentation. Game titles, artwork
and visuals remain the property of their respective rights holders. The website
Credits page states this explicitly.

The project does not distribute BIOS files, game ROMs, C7010 firmware,
commercial manuals or commercial box art.

### 4.5 Game ROMs, BIOS and firmware

No BIOS files, game ROMs, C7010 firmware, commercial manuals or commercial
box art are distributed in the O2EM-NG packages. The package contains only
placeholder `README.txt` files in `BIOS`, `ROMS`, `CARTRIDGES`, `MANUALS` and
`BOXART`. This was verified against the v5.0.1 package and is preserved for
v5.0.2.

---

## 5. Original O2EM inheritance

Original O2EM copyright notice, as published in the O2EM 1.18 README:

```text
copyright 1996/1998 by Daniel Boris <dboris@comcast.net>

Developed by Daniel Boris, Andre de la Rocha and Arlindo Oliveira

This package is released under the Clarified Artistic License (see COPYING)
```

O2EM-NG is a modified and continued work. It preserves the Clarified Artistic
License and the original notices. Modified O2EM-derived files now carry a
prominent modification notice as required by section 3 of that licence.

Historical contributors and technical references — including Mark Guttenbrunner
(Manopac), Sören Gust and Brian Dehli — are acknowledged in `Docs/CREDITS.md`.
They are not listed as copyright holders unless separately established.

---

## 6. O2EM-NG independent development

The following are O2EM-NG work, not MAME or original O2EM code:

- The MCS48-NG CPU implementation.
- The NG machine scheduler, timebase and host boundary.
- The NG audio device.
- The C7010 Chess Module and NSC800 implementation.
- The NG modular machine architecture and cartridge mapper classes.
- The Windows frontend, settings, import centre, media viewer and controller
  integration.
- The Game Library, catalogue migration, My Real Collection, favorites, ratings
  and play history.
- The v5.0.2 licensing, provenance and credits documentation.

This list is a description of project work, not a claim of clean-room
implementation for mixed-provenance components such as the VDC and VPP.

---

## 7. Unresolved provenance and legal questions

1. Copyright status of the original i8244/i8245 character generator ROM data and
   the scope of the O2EM/MAME licence grants over it.
2. Rights position for the promotional gameplay images and videos used on the
   official website. They are not bundled in release packages.
3. Microsoft Visual C++ redistributable notice requirements for the specific
   build and distribution method.
4. Whether any external C7010 ancestry exists outside the local O2EM 1.18 tree.
5. The precise boundary of MAME adaptation in mixed-provenance VDC/VPP files,
   beyond the functions and blocks identified by the audit.

These items are recorded as open questions. They are not presented as confirmed
violations. Qualified legal review is recommended where a definitive answer is
required.

---

## 8. Maintenance rule

When code or data is added or changed:

1. Update this file and `Docs/Development/NG-0_Provenance.md`.
2. Add the applicable notice to source headers.
3. Update `Docs/THIRD_PARTY_NOTICES.md` and `Docs/CREDITS.md` if the change
   affects attribution.
4. Include any new licence text in `Docs/Licenses/` and in the release manifest.
5. Re-check that the installer and portable package contain the notices.
