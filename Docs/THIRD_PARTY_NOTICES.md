# Third-Party Notices

O2EM-NG contains original O2EM heritage, independently developed O2EM-NG
code, adapted portions of third-party source code and third-party libraries.
Different components are governed by different licences. This file records the
third-party components and the notices that apply to them.

The full licence texts are in `Docs/Licenses/`. The component-level provenance
register is `Docs/SOURCE_PROVENANCE.md`. The central credits are
`Docs/CREDITS.md`.

---

## 1. MAME — adapted source code

O2EM-NG contains portions adapted from MAME source code licensed under the
**BSD 3-Clause License**. The adapted portions are not a copy of the whole MAME
files; the surrounding O2EM-NG implementations are restructured.

| O2EM-NG component | MAME source | Licence | Adapted element |
|---|---|---|---|
| `src/core_ng/video/vdc.cpp`, `vdc.h`, `vdc_state.h`, `i8244.cpp`, `i8245.cpp` | `src/devices/video/i8244.cpp`, `i8244.h` (MAME 0.289) | BSD-3-Clause | Register mirroring, register read/write masks and grouping, VBL/timing behaviour, and the 16-colour palette ordering |
| `src/core_ng/video/vpp.cpp`, `vpp.h` | `src/devices/video/ef9340_1.cpp`, `ef9340_1.h` (MAME 0.289) | BSD-3-Clause | Scanline character drawing structure: `vpos`/`zoom`/`slice`, character data selection, double width/height, underline, blink, cursor, invert and delimiter handling |
| `src/core_ng/audio/audio_device.cpp` | `src/devices/video/i8244.cpp` (MAME 0.289), sound section | BSD-3-Clause | Behavioural reference for the shift register, HBLANK prescaler and sound IRQ. The implementation is independently structured. |
| Machine, bus and cartridge behaviour | `src/mame/philips/odyssey2.cpp`, `src/devices/bus/odyssey2/*`, `src/emu/screen.cpp` (MAME 0.289) | BSD-3-Clause | Behavioural reference for memory map, MOVX decode, P1/P2, IRQ/T1, banking and timing. No long verbatim blocks identified. |

Reference revision:

```text
MAME 0.289
Pinned commit: 6512b9adc62a2458c412342268f500f8489edb95
  src/devices/video/i8244.cpp
  src/devices/video/i8244.h
  src/devices/video/ef9340_1.cpp
  src/devices/video/ef9340_1.h
```

MAME copyright holders for the referenced files:

- Intel 8244/8245: **Peter Trauner, Wilbert Pol, hap**
- Thomson EF9340/EF9341: **Wilbert Pol, hap**
- Additional acknowledgement in the i8244 file: **Kevin Horton**

MAME as a whole is distributed under GPL-2.0-or-later, but the referenced files
carry an explicit per-file `license:BSD-3-Clause` declaration. The BSD 3-Clause
grant applies to the adapted portions. The complete licence text and disclaimer
are in [`Licenses/LICENSE.MAME-BSD-3-Clause.txt`](Licenses/LICENSE.MAME-BSD-3-Clause.txt).

MAME is a registered trademark of Gregory Ember. O2EM-NG is not affiliated
with, endorsed by, or sponsored by MAME or its contributors. The names of MAME
and its contributors may not be used to endorse or promote O2EM-NG without
specific prior written permission.

---

## 2. Embedded character data and palette values

| O2EM-NG file | Data | Documented origin | Status |
|---|---|---|---|
| `src/core_ng/video/character_rom.cpp` | 512-byte i8244/i8245 character generator ROM, CRC32 `B46A3F31`, SHA1 `415382715455b47b69401b3d60bd8f0036dd7fef` | Original O2EM `cset.c` (identical bytes, same CRC32) and MAME 0.289 `charset_i8244.bin` (identical dump) | Distributed with original O2EM under the Clarified Artistic License since the 1990s; also present in MAME under BSD-3-Clause. See `SOURCE_PROVENANCE.md` for the unresolved question about copyright in the underlying silicon data. |
| `src/core_ng/video/ef9340_charset.cpp` | 2,560-byte EF9340/EF9341 character generator table, CRC32 `D557A7BF` | Original O2EM `vpp_cset.cpp` | Inherited from O2EM under the Clarified Artistic License. MAME's datasheet-derived image has a different CRC and is marked `BAD_DUMP`; it is not used. |
| `src/core_ng/video/vdc.cpp` (palette table) | 16-colour i8244 palette | MAME 0.289 `i8244_palette` ordering; the same colour values are present in original O2EM `vdc.c` `colortable[1]` in a different index order | BSD-3-Clause notice applies to the MAME palette ordering. The colour values are hardware colour definitions. |

These notices do not assert that MAME's source-code licence automatically grants
rights in original hardware ROM contents. The project's position and the
remaining legal question are recorded in `SOURCE_PROVENANCE.md`.

---

## 3. SDL3, SDL3_image and SDL3_ttf

| Library | Copyright | Licence | Notice file |
|---|---|---|---|
| SDL3 | Sam Lantinga and SDL contributors | zlib | `Licenses/LICENSE.SDL3.txt` |
| SDL3_image | Sam Lantinga and SDL contributors | zlib | `Licenses/LICENSE.SDL3_image.txt` |
| SDL3_ttf | Sam Lantinga and SDL contributors | zlib | `Licenses/LICENSE.SDL3_ttf.txt` |
| FreeType (bundled inside SDL3_ttf.dll) | David Turner, Robert Wilhelm, Werner Lemberg and FreeType contributors | FreeType License (FTL) | `Licenses/LICENSE.FreeType.txt` |
| HarfBuzz (bundled inside SDL3_ttf.dll) | Google, Inc. and HarfBuzz contributors | Old MIT | `Licenses/LICENSE.HarfBuzz.txt` |

The zlib licence requires that the origin of the software is not
misrepresented, that altered versions are marked as altered, and that the
notice is not removed. The DLLs are distributed unmodified.

`SDL3_ttf.dll` is a combined binary that statically includes FreeType and
HarfBuzz. Their notices are included even though they are not separate files in
the package. The FreeType licence requires credit to the FreeType Project; the
HarfBuzz licence requires preservation of its copyright and permission notice.

`SDL3_image.dll` is distributed without the optional codec DLLs
(`libavif-16.dll`, `libpng16-16.dll`, `libtiff-6.dll`, `libwebp*.dll`). Those
optional codecs are not part of the O2EM-NG package. If they are added later,
their notices must be added as well.

Projects:

- https://www.libsdl.org/
- https://freetype.org/
- https://harfbuzz.github.io/

---

## 4. PDFium

| Library | Copyright | Licence | Notice file |
|---|---|---|---|
| PDFium | PDFium Authors | MIT (with third-party components) | `Licenses/LICENSE.pdfium.txt` |

Project: https://pdfium.googlesource.com/pdfium/

The distributed `pdfium.dll` is used for PDF manual preview. The notice file is
the one supplied with the binary used by the build.

---

## 5. Microsoft Visual C++ runtime

The Windows packages include `msvcp140.dll`, `vcruntime140.dll` and
`vcruntime140_1.dll` (file version 14.52.37028.0). These are Microsoft
redistributable runtime components, not part of O2EM-NG and not covered by the
O2EM-NG licence. Their use and redistribution are governed by the Microsoft
Visual Studio license terms. See
[`Licenses/LICENSE.Microsoft-VCRuntime.txt`](Licenses/LICENSE.Microsoft-VCRuntime.txt).

---

## 6. SQLite

O2EM-NG uses the SQLite API through the Windows-supplied `winsqlite3.dll`.
SQLite itself is in the public domain. The Windows DLL is a system component
and is not redistributed in the O2EM-NG packages.

Project: https://www.sqlite.org/

---

## 7. Build tools

O2EM-NG is built with Microsoft Visual Studio and the Windows SDK. These
tools are not redistributed and are not part of the O2EM-NG runtime package.
The distributed Microsoft runtime DLLs are covered by section 5.

---

## 8. Original O2EM

The original O2EM emulator is not a third-party library in the same sense as
SDL3 or PDFium: O2EM-NG is a continuation of O2EM. Original O2EM code and data
remain under the **Clarified Artistic License** with the original notices.
See `Docs/COPYRIGHTS.txt`, `Docs/CREDITS.md` and `Docs/SOURCE_PROVENANCE.md`.

Original O2EM project: http://o2em.sourceforge.net/

---

## 9. What this file does not do

- It does not replace the full licence texts in `Docs/Licenses/`.
- It does not relicense third-party code as O2EM-NG-owned code.
- It does not claim that O2EM-NG is endorsed by MAME, SDL, PDFium, Microsoft or
  the original O2EM project.
- It does not resolve the underlying copyright status of original hardware ROM
  data or the rights position for promotional gameplay images on the website.
  Those open questions are recorded in `Docs/SOURCE_PROVENANCE.md`. Release
  packages do not bundle gameplay screenshots or videos.
