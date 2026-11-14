# O2EM-NG Beta 5 - v5.0.1 UI Update

## Introduction

O2EM-NG Beta 5 v5.0.1 is a user-interface and usability update to Beta 5. It
keeps the NG emulation architecture, C7010 support, audio engine and timing
unchanged while improving everyday use of the Windows frontend and game
library.

This is a UI update, not a new emulation-engine release.

## User interface improvements

- **Edit Game Data:** corrected mouse row activation. Rendering and hit-testing
  now use the same row geometry, so clicking a metadata field selects the
  intended row.
- **Edit Game Data input focus:** Game Library input is locked while the editor
  is open. Mouse-wheel input no longer scrolls the underlying library or
  accidentally changes the game being edited.
- **About page:** expanded with project background, O2EM heritage, NG
  architecture, supported hardware, preservation goals and project links. The
  Windows 95 visual identity is unchanged.
- **Check Updates:** improved JSON parsing and error handling. Invalid responses
  and network failures are no longer reported as successful update checks. The
  existing asynchronous WinHTTP implementation and numeric version comparison
  policy are retained.

## Game Library improvements

- **Recently Played:** the library automatically scrolls to the selected game.
  Existing selection behavior is unchanged.
- **Favorites:** the library automatically scrolls to the selected favorite.
  Entries that are already visible do not cause unnecessary movement.
- **START GAME:** a new button below DELETE in Cover / Media launches the
  selected game through the existing launch function and is disabled when the
  selected game cannot be launched.

## My Real Collection improvements

- The user-facing **My Collection** feature is now named **My Real Collection**.
  Existing collection databases and records are preserved.
- **Direct collection editing:** Game Details can be edited without opening the
  separate Edit Entry dialog. Existing persistence and validation logic is
  reused, and changes remain saved after closing and restarting O2EM-NG.

## Installation and upgrade

The Windows installer installs for the current user under
`%LOCALAPPDATA%\Programs\O2EM-NG` and does not require administrator rights.

Upgrading an existing v5.0.0 installation preserves the user game database,
custom metadata, collection records, favorites, ratings, settings, ROMs, BIOS
files, media and save data. A new installation receives the supplied catalogue
seed. The installer uses the established application identity and user install
location, so existing shortcuts continue to point to the installed executable.

Keep a backup of your personal data before any major upgrade, as with any
Windows application.

## Known minor limitation

The mouse wheel does not scroll inside Edit Game Data. Game Library input is
locked while the editor is open, preventing accidental changes to the selected
game. This limitation is accepted for v5.0.1.

## Emulation engine status

The NG emulation engines are unchanged in this update. C7010 Chess support,
audio, video, timing and existing ROM compatibility behavior are the same as
v5.0.0. No new emulation compatibility claims are made for this release.
