# O2EM-NG Beta 5 - v5.0.2 Licensing & Attribution Update

## Introduction

O2EM-NG Beta 5 v5.0.2 is a licensing, copyright, attribution and source
provenance update. It corrects notices, adds the required third-party licence
texts, records component-level provenance and expands the project credits.

The NG emulation engines, C7010 support, audio, video, timing, input, databases
and collections are unchanged.

## Copyright and licence corrections

- Restored the exact original O2EM copyright notice in `Docs/COPYRIGHTS.txt`
  alongside the unchanged Clarified Artistic License text.
- Added prominent Clarified Artistic License section 3 modification notices to
  the modified O2EM-derived source files.
- Added the complete MAME BSD 3-Clause notice, copyright holders and disclaimer
  in `Docs/Licenses/LICENSE.MAME-BSD-3-Clause.txt`.
- Added accurate MAME provenance headers to the adapted VDC/VPP source files and
  the embedded i8244 character ROM.
- Added the O2EM provenance header to the embedded EF9340 character table.
- Corrected the earlier "independent implementation / no verbatim source copy"
  claims for the VDC/VPP portions identified by the audit.

## Third-party notices

- Added `Docs/Licenses/LICENSE.SDL3_ttf.txt`.
- Added `Docs/Licenses/LICENSE.Microsoft-VCRuntime.txt`.
- Added `Docs/THIRD_PARTY_NOTICES.md` with a component-by-component mapping.
- Verified the SDL3, SDL3_image and PDFium notices in the distributed package.

## Provenance and credits

- Added `Docs/SOURCE_PROVENANCE.md` as the permanent component-level register.
- Added `Docs/CREDITS.md` as the canonical credits source.
- Added a root `LICENSE` component-licensing index and a root `README.md`.
- Documented the 512-byte i8244 character ROM's dual provenance: identical bytes
  exist in original O2EM `cset.c` (CRC32 `B46A3F31`) and in MAME's dump.
- Documented the EF9340 character table's O2EM provenance.
- Recorded the remaining open questions: underlying hardware-ROM copyright,
  gameplay screenshot/video rights, and Microsoft runtime notice requirements.

## Application and website

- Expanded the About, Credits and Special Thanks pages.
- Release Notes now describe the licensing update.
- Added a dedicated Credits page to the official website at
  https://o2em-ng.eu/credits/.
- Added Credits to the website's top-level navigation.
- Release packages no longer bundle gameplay screenshots or videos. The
  `SCREENSHOTS` folder is created for user-supplied media. The website uses
  separate promotional images with an explicit rights note.

## Installation and upgrade

The Windows installer installs for the current user under
`%LOCALAPPDATA%\Programs\O2EM-NG` and does not require administrator rights.

Upgrading an existing v5.0.0 or v5.0.1 installation preserves the user game
database, custom metadata, collection records, favorites, ratings, settings,
ROMs, BIOS files, media and save data. A new installation receives the supplied
catalogue seed.

Keep a backup of your personal data before any major upgrade, as with any
Windows application.

## Emulation engine status

The NG emulation engines are unchanged in this update. C7010 Chess support,
audio, video, timing, input and existing ROM compatibility behaviour are the
same as v5.0.1. No new emulation compatibility claims are made for this release.

## Open questions

The following are documented in `Docs/SOURCE_PROVENANCE.md` and are not
presented as confirmed violations:

- copyright status of original hardware character data;
- rights position for promotional gameplay images used on the official website
  (not bundled in release packages);
- Microsoft Visual C++ redistributable notice requirements.

These questions do not affect emulation behaviour.
