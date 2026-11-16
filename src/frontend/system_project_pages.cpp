#include "system_project_pages.h"
#include "src/version.h"

namespace {

const char* kAbout = "O2EM-NG\n" O2EM_RELEASE_NAME " - v" O2EM_VERSION_STRING "\n\n"
    R"TXT(HERITAGE
O2EM-NG builds on O2EM, the Odyssey2 / Videopac emulator created by Daniel Boris in 1997 and developed by Andre de la Rocha and Arlindo M. de Oliveira. The original project's work and credits remain part of our heritage.

PRESERVATION AND PURPOSE
Developed by Bengt-Ove Peltz, O2EM-NG combines a practical Windows frontend with ongoing preservation of Philips Videopac and Magnavox Odyssey2 games, documentation and hardware knowledge. Its familiar Windows 95 presentation keeps the library, media and personal collection close at hand.

THE NG GENERATION
Beta 5 brings the NG machine architecture: a deterministic scheduler, MCS-48 processor, i8244/i8245 video, synchronized audio and integrated expansion hardware. The frontend and library data remain separate from the emulation engines.

SOURCE PROVENANCE
O2EM-NG contains original O2EM heritage, independently developed O2EM-NG components, permitted adaptations of MAME BSD-3-Clause source code and third-party libraries. The independently developed components include the MCS48-NG CPU, the NG machine architecture, the audio device and the C7010 Chess Module. Parts of the Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP implementations are adapted from MAME. See Docs/SOURCE_PROVENANCE.md and Docs/THIRD_PARTY_NOTICES.md.

HARDWARE
The project includes G7000 / Odyssey2, G7400 / Videopac+, Jopac, C7010 Chess Module and The Voice support. BIOS, expansion firmware and voice samples are required where applicable and are supplied separately. Compatibility varies by title and hardware feature; this is not a claim of perfect emulation.

PROJECT AND COPYRIGHT
O2EM-NG development: Copyright (c) 2026 Bengt-Ove Peltz. Official source, releases and issue tracker: github.com/bengtovepeltz-design/O2EM-NG (link above).

Original O2EM material retains its Clarified Artistic License and original notices. MAME-adapted portions retain the BSD-3-Clause notice. See Docs/COPYRIGHTS.txt, Docs/Licenses/, Docs/CREDITS.md and Docs/SOURCE_PROVENANCE.md. Credits and Special Thanks remain available on their own pages.)TXT";


const char* kCredits = R"TXT(O2EM-NG DEVELOPMENT

Bengt-Ove Peltz
O2EM-NG Project
https://o2em-ng.eu

Project direction and independently developed O2EM-NG work:
- MCS48-NG Intel 8048 CPU implementation
- NG machine scheduler, timebase and modular architecture
- NG audio device
- C7010 Chess Module and NSC800 implementation
- NG cartridge mappers and bus integration
- Windows frontend, settings, import centre, media viewer and controller support
- Game Library, catalogue migration, My Real Collection, favorites, ratings and play history
- Build, packaging, release engineering, testing and documentation

Additional O2EM-NG contributors are recorded in the project Git history.


ORIGINAL O2EM PROJECT

O2EM Free Odyssey2 / Videopac+ Emulator
http://o2em.sourceforge.net/

Original copyright notice:
copyright 1996/1998 by Daniel Boris <dboris@comcast.net>
Developed by Daniel Boris, Andre de la Rocha and Arlindo Oliveira
Released under the Clarified Artistic License.

Original developers:
- Daniel Boris - creator of O2EM and original 8048/VDC emulation
- Andre de la Rocha - O2EM development from version 0.80
- Arlindo M. de Oliveira - O2EM development from version 1.16

Historical contributors and technical references:
- Mark Guttenbrunner (Manopac) - original O2EM work and Videopac/Odyssey2 hardware research
- Soren Gust - hardware documentation, MegaCART, Videopac+ research and compatibility work
- Heitor Barcellos - original O2EM Launcher frontend

These contributors are acknowledged for their work and are not listed here as copyright holders unless separately established.


MAME PROJECT AND CONTRIBUTORS

MAME - Multiple Arcade Machine Emulator
https://www.mamedev.org/

Parts of the O2EM-NG Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP implementations have been adapted from MAME's BSD-3-Clause licensed source code. The surrounding O2EM-NG implementations are restructured; this is not a copy of the complete MAME files.

Reference: MAME 0.289, commit 6512b9adc62a2458c412342268f500f8489edb95
  src/devices/video/i8244.cpp / i8244.h
  src/devices/video/ef9340_1.cpp / ef9340_1.h

MAME copyright holders:
- Peter Trauner - Intel 8244/8245 VDC
- Wilbert Pol - Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP
- hap - Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP

Additional acknowledgement:
- Kevin Horton - acknowledged in the MAME i8244 source

MAME is a registered trademark of Gregory Ember. O2EM-NG is not affiliated with, endorsed by, or sponsored by MAME or its contributors.


THIRD-PARTY SOFTWARE

SDL3, SDL3_image, SDL3_ttf - Sam Lantinga and SDL contributors - zlib license
PDFium - PDFium Authors - MIT and third-party notices
Microsoft Visual C++ runtime - Microsoft Corporation - Microsoft license terms
SQLite - public domain; Windows winsqlite3.dll is a system component and is not redistributed

Complete notices: Docs/THIRD_PARTY_NOTICES.md and Docs/Licenses/


COPYRIGHT, LICENSING AND SOURCE PROVENANCE

O2EM-NG contains original O2EM heritage, independently developed O2EM-NG code, permitted MAME BSD-3-Clause adaptations and third-party libraries.

Project documents:
- Docs/COPYRIGHTS.txt
- Docs/CREDITS.md
- Docs/SOURCE_PROVENANCE.md
- Docs/THIRD_PARTY_NOTICES.md
- Docs/Licenses/

Repository: https://github.com/bengtovepeltz-design/O2EM-NG
Original O2EM: http://o2em.sourceforge.net/
MAME: https://www.mamedev.org/

Special Thanks are listed on their own page.)TXT";

const char* kSpecialThanks = R"TXT(Videopac and Odyssey2 communities

Collectors and archivists

Mark Guttenbrunner (Manopac)
For his extensive work on the original O2EM emulator and his invaluable contributions to understanding and preserving the Philips Videopac/Odyssey2 hardware. His research and emulation work remains an important technical reference for O2EM-NG.

Brian Dehli - Tvspil.dk
For his generous support of the O2EM-NG project, including assistance in locating rare Videopac documentation, sharing his extensive knowledge, and contributing original Videopac cartridges and other physical reference material for preservation and testing. His long-standing dedication to preserving Philips Videopac hardware, games and history through Tvspil.dk - and his remarkable generosity and willingness to help - are sincerely appreciated.

Soren Gust
For his important technical contributions to O2EM and the wider Videopac community, including detailed hardware documentation, emulator patches, MegaCART bank-switching support, Videopac+ research and numerous improvements to compatibility and accuracy. Parts of his work remain part of the technical heritage inherited by O2EM-NG, and his documentation continues to be a valuable reference for understanding the Philips Videopac hardware.

Beta testers and community reporters
For testing games, hardware configurations, controllers and edge cases, and for reporting reproducible issues.

These acknowledgements recognise research, documentation, testing and technical assistance. They are not statements of copyright ownership unless a separate copyright notice establishes that status.)TXT";

const char* kRoadmap = R"TXT(BETA 5
NG emulation core
G7000 / G7400 convergence
C7010 special hardware
Licensing, provenance and credits (v5.0.2)
Compatibility and regression testing

FUTURE
Add planned features and priorities here.)TXT";

const char* kReleaseNotes = "O2EM-NG Beta 5 - v" O2EM_VERSION_STRING " Licensing & Attribution Update\n\n"
    R"TXT(This is a licensing, copyright, attribution and source-provenance update. The NG emulation engines, C7010 support, audio, video, timing, input, databases and collections are unchanged.

What's new in v5.0.2:

- Restored the original O2EM copyright notice and Clarified Artistic License section 3 modification notices.
- Added the complete MAME BSD-3-Clause notice for the adapted Intel 8244/8245 VDC and Thomson EF9340/EF9341 VPP portions.
- Added third-party notices and licence texts, including SDL3_ttf and the Microsoft Visual C++ runtime notice.
- Added Docs/SOURCE_PROVENANCE.md with component-level provenance.
- Added Docs/CREDITS.md as the canonical credits source.
- Expanded the About, Credits and Special Thanks pages.
- Added a root LICENSE index and a root README.
- Added a dedicated Credits page on the official website.
- Documented embedded character ROM provenance, including the identical original O2EM cset.c table.

Compatibility:

- Existing v5.0.0 and v5.0.1 installations upgrade in place.
- User database, My Real Collection, favorites, ratings, play history, settings, ROMs, BIOS and media are preserved.

Known unresolved questions:

- Copyright status of original hardware character data.
- Rights position for gameplay screenshots and videos in packages.
- Microsoft Visual C++ redistributable notice requirements.

These are documented in Docs/SOURCE_PROVENANCE.md. They do not change emulation behaviour.

Older release history remains available in Docs/CHANGELOG.md.)TXT";

} // namespace

std::vector<ProjectPage> BuildSystemProjectPages()
{
    return {
        { "about", "About", kAbout, 0 },
        { "credits", "Credits", kCredits, 1 },
        { "special_thanks", "Special Thanks", kSpecialThanks, 2 },
        { "roadmap", "Roadmap", kRoadmap, 3 },
        { "release_notes", "Release Notes", kReleaseNotes, 4 },
    };
}
