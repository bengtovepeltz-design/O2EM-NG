# Credits and Acknowledgements

Canonical credits source for O2EM-NG.

This document is the authoritative reference for the application About, Credits
and Special Thanks pages, the official website Credits page, the GitHub
documentation and the distribution documentation. It distinguishes project
development, original O2EM heritage, MAME-derived work, third-party libraries and
special thanks. Acknowledgement does not replace the legally required copyright
and licence notices in `Docs/COPYRIGHTS.txt`, `Docs/THIRD_PARTY_NOTICES.md` and
`Docs/Licenses/`.

---

## 1. O2EM-NG Development

**Bengt-Ove Peltz**
O2EM-NG Project
https://o2em-ng.eu

Project direction and independently developed O2EM-NG work:

- The MCS48-NG Intel 8048 CPU implementation.
- The NG machine scheduler, timebase, host boundary and modular machine
  architecture.
- The NG audio device.
- The C7010 Chess Module and NSC800 implementation.
- The NG cartridge mapper classes and cartridge/bus integration.
- The Windows frontend, settings, import centre, media viewer and
  Xbox-compatible controller integration.
- The Game Library, catalogue migration, My Real Collection, favorites,
  personal ratings and play history.
- Build, packaging, release engineering, testing, documentation and the
  v5.0.2 licensing and provenance work.

Additional O2EM-NG contributors are recorded in the project Git history.

---

## 2. Original O2EM Project

O2EM-NG continues and extends the original O2EM emulator.

Original O2EM project: http://o2em.sourceforge.net/

**Original copyright notice:**

```text
copyright 1996/1998 by Daniel Boris <dboris@comcast.net>

Developed by Daniel Boris, Andre de la Rocha and Arlindo Oliveira

This package is released under the Clarified Artistic License (see COPYING)
```

**Original developers:**

- **Daniel Boris** — creator of O2EM and the original 8048/VDC emulation work.
- **Andre de la Rocha** — O2EM development from version 0.80.
- **Arlindo M. de Oliveira** — O2EM development from version 1.16.

**Historical contributors and technical references:**

- **Mark Guttenbrunner (Manopac)** — acknowledged for extensive work on the
  original O2EM emulator and for preserving and explaining Philips
  Videopac/Odyssey² hardware. His research and emulation work remain an
  important technical reference for O2EM-NG.
- **Sören Gust** — acknowledged for hardware documentation, emulator patches,
  MegaCART bank-switching support, Videopac+ research and compatibility work.
  Parts of that work remain part of the technical heritage inherited by
  O2EM-NG.
- **Heitor Barcellos** — creator of the original O2EM Launcher frontend.

These historical contributors are acknowledged for their work. They are not
listed here as copyright holders unless a separate copyright notice establishes
that status.

Original O2EM code and data remain under the Clarified Artistic License. See
`Docs/COPYRIGHTS.txt`.

---

## 3. MAME Project and Contributors

**MAME — Multiple Arcade Machine Emulator**
https://www.mamedev.org/

Parts of the O2EM-NG Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP
implementations have been adapted from MAME's BSD-3-Clause licensed source
code. The surrounding O2EM-NG implementations are restructured; this is not a
copy of the complete MAME files.

Reference revision:

```text
MAME 0.289
Pinned commit: 6512b9adc62a2458c412342268f500f8489edb95
  src/devices/video/i8244.cpp / i8244.h
  src/devices/video/ef9340_1.cpp / ef9340_1.h
```

**Copyright holders:**

- **Peter Trauner** — Intel 8244/8245 VDC.
- **Wilbert Pol** — Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP.
- **hap** — Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP.

**Additional acknowledgement:**

- **Kevin Horton** — acknowledged in the MAME i8244 source (`thanks-to`).

MAME as a whole is distributed under GPL-2.0-or-later, but the referenced files
carry an explicit per-file BSD-3-Clause declaration. The complete BSD 3-Clause
text and disclaimer are in
`Docs/Licenses/LICENSE.MAME-BSD-3-Clause.txt`.

MAME is a registered trademark of Gregory Ember. O2EM-NG is not affiliated with,
endorsed by, or sponsored by MAME or its contributors.

---

## 4. Special Thanks

**Videopac and Odyssey² communities**
For testing, documentation, preservation work, hardware knowledge and continued
interest in the platform.

**Collectors and archivists**
For preserving hardware, cartridges, manuals and documentation.

**Brian Dehli — Tvspil.dk**
For generous support of the O2EM-NG project, including assistance in locating
rare Videopac documentation, sharing knowledge, and contributing original
Videopac cartridges and physical reference material for preservation and
testing. His long-standing dedication to preserving Philips Videopac hardware,
games and history through Tvspil.dk is sincerely appreciated.

**Sören Gust**
For important technical contributions to O2EM and the wider Videopac community,
including detailed hardware documentation, emulator patches, MegaCART
bank-switching support, Videopac+ research and numerous improvements to
compatibility and accuracy. Parts of his work remain part of the technical
heritage inherited by O2EM-NG, and his documentation continues to be a valuable
reference.

**Mark Guttenbrunner (Manopac)**
For extensive work on the original O2EM emulator and for contributions to
understanding and preserving Philips Videopac/Odyssey² hardware. His research
and emulation work remain an important technical reference for O2EM-NG.

**Beta testers and community reporters**
For testing games, hardware configurations, controllers and edge cases, and for
reporting reproducible issues.

---

## 5. Third-Party Libraries

| Component | Project | Copyright | Licence | Notice |
|---|---|---|---|---|
| SDL3 | https://www.libsdl.org/ | Sam Lantinga and SDL contributors | zlib | `Docs/Licenses/LICENSE.SDL3.txt` |
| SDL3_image | https://www.libsdl.org/projects/SDL_image/ | Sam Lantinga and SDL contributors | zlib | `Docs/Licenses/LICENSE.SDL3_image.txt` |
| SDL3_ttf | https://www.libsdl.org/projects/SDL_ttf/ | Sam Lantinga and SDL contributors | zlib | `Docs/Licenses/LICENSE.SDL3_ttf.txt` |
| FreeType (in SDL3_ttf) | https://freetype.org/ | David Turner, Robert Wilhelm, Werner Lemberg and contributors | FreeType License | `Docs/Licenses/LICENSE.FreeType.txt` |
| HarfBuzz (in SDL3_ttf) | https://harfbuzz.github.io/ | Google, Inc. and contributors | Old MIT | `Docs/Licenses/LICENSE.HarfBuzz.txt` |
| PDFium | https://pdfium.googlesource.com/pdfium/ | PDFium Authors | MIT and third-party notices | `Docs/Licenses/LICENSE.pdfium.txt` |
| Microsoft Visual C++ runtime | https://visualstudio.microsoft.com/ | Microsoft Corporation | Microsoft Visual Studio license terms | `Docs/Licenses/LICENSE.Microsoft-VCRuntime.txt` |
| SQLite (Windows `winsqlite3.dll`) | https://www.sqlite.org/ | SQLite authors / Microsoft Windows | Public domain / system component | Not redistributed |
| Microsoft Visual Studio and Windows SDK | https://visualstudio.microsoft.com/ | Microsoft Corporation | Microsoft license terms | Build tools only; not redistributed |

---

## 6. Copyright, Licensing and Source Provenance

O2EM-NG contains a combination of:

- original O2EM heritage under the Clarified Artistic License;
- independently developed O2EM-NG code;
- permitted adaptations of MAME BSD-3-Clause source code;
- third-party libraries under their respective licences.

Project documents:

- `Docs/COPYRIGHTS.txt` — Clarified Artistic License and original O2EM notice.
- `Docs/THIRD_PARTY_NOTICES.md` — third-party notice mapping.
- `Docs/SOURCE_PROVENANCE.md` — component-level provenance.
- `Docs/Licenses/` — full licence texts.

External links:

- O2EM-NG repository: https://github.com/bengtovepeltz-design/O2EM-NG
- Original O2EM project: http://o2em.sourceforge.net/
- MAME project: https://www.mamedev.org/
- MAME i8244 source: https://github.com/mamedev/mame/blob/master/src/devices/video/i8244.cpp
- MAME ef9340_1 source: https://github.com/mamedev/mame/blob/master/src/devices/video/ef9340_1.cpp

---

## 7. Maintenance rule

Credits and provenance must remain consistent across all public locations.

For every future release:

1. Update this file first.
2. Synchronise the application About, Credits and Special Thanks pages.
3. Synchronise the website Credits page and the relevant About/Downloads pages.
4. Synchronise the GitHub README and release notes.
5. Synchronise `Docs/readme.md`, `Docs/Project.md`, `Docs/THIRD_PARTY_NOTICES.md`
   and `Docs/SOURCE_PROVENANCE.md`.
6. Verify the installer and portable package contain the updated documents.

User-friendly acknowledgements do not replace the exact copyright and licence
notices.
