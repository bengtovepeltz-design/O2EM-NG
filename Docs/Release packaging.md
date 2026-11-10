# Release packaging

Build **Release | x64** in Visual Studio. After a successful build, `PackageRelease`
creates `dist/O2EM-NG-Beta-5-v5.0.0/` and a matching ZIP for the current
development candidate.

Edit `tools/release-manifest.json` to choose files, destinations, empty user folders,
the version number, release name and approved seed hash. Relative sources are relative to the project directory;
absolute sources currently point to this project's SDL SDK installation.
The executable comes from MSBuild's TargetPath and the C++ runtime from its
VCInstallDir redistributable folders. Missing required files fail the packaging step rather than
silently creating an incomplete distribution.

The authoritative seed is always `Gamedata/o2em-ng.db`. A Release build also stages
that exact file into `x64/Release/GAMEDATA/o2em-ng.db` through
`tools/stage-release-seed.ps1`, so running the Release build folder directly uses the
same seed as the package. Any previous runtime database in that build-output folder is
backed up as `o2em-ng.runtime-backup-<timestamp>.db` before it is replaced.

`tools/package-release.ps1` verifies both the tracked seed and the packaged
`GAMEDATA/o2em-ng.db` against the SHA256 in `release-manifest.json`. A mismatch fails
the package instead of producing a wrong release. `x64/Release/GAMEDATA` is never used
as the package source.

A clean installation receives this seed. Upgrading an existing installation preserves
the existing user `GAMEDATA/o2em-ng.db` by design; only a clean destination is
guaranteed to start with the approved seed and empty My Collection.

Only listed files and the approved content folders are included. The supplied
GAMEDATA/o2em-ng.db catalogue is included. No local configuration, ROMs, BIOS,
firmware, PDB files or build logs are copied. The application uses the supplied
catalogue and creates its own configuration.

The approved O2EM-NG screenshots prepared in `x64/Release/SCREENSHOTS` are copied
to the package `SCREENSHOTS` folder. Only media extensions (`.png`, `.jpg`,
`.jpeg`, `.webp`, `.gif`, `.mp4`) are taken from that folder. Temporary frame
dumps, MAME/test captures, diff images, probe output and debugger screenshots are
not part of the packaging source and are never copied from `tests/` or local
capture folders. Empty content folders include README files.
Each package has VERSION.txt and SHA256SUMS.txt.

A previous package/ZIP is preserved with a `.previous-<timestamp>` suffix.
Do not use the generated distribution as your permanent playing folder.
Debug builds are unaffected. To build Release without packaging, pass
`/p:SkipReleasePackaging=true` to MSBuild.

After packaging, run `Installer/Build Installer.bat` with Inno Setup 7 or 6 installed.
The installer reads `dist/release-info.iss`, generated from the same manifest version
and release name, and consumes the prepared package. The current candidate output is
`O2EM-NG-Beta-5-v5.0.0-Setup.exe`. It does not copy your development configuration.
The supplied database seeds new installations; an existing user database is preserved
on upgrade and uninstall. Upgrades do not automatically merge new catalogue entries.
The application icon is embedded in the executable and included for the SDL window.

Packaging does not publish to GitHub. Test the package and installer on a clean
Windows PC before distributing them.

The installer installs for the current user in %LOCALAPPDATA%\Programs\O2EM-NG without administrator rights.
This is required because imports, the catalogue and settings are stored beside the executable.
It does not reuse the earlier Program Files installation directory. Existing data in an old
Program Files installation is not moved or removed automatically; copy any personal data
to the new installation before removing the old copy. Choose a writable folder for portable ZIP use too.
