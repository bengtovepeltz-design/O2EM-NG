# Release packaging

Build **Release | x64** in Visual Studio. After a successful build, `PackageRelease`
creates `dist/O2EM-NG-v0.31.0-beta/` and a matching ZIP.

Edit `tools/release-manifest.json` to choose files, destinations, empty user folders
and the version number. Relative sources are relative to the project directory;
absolute sources currently point to this project's SDL SDK installation.
The executable comes from MSBuild's TargetPath and the C++ runtime from its
VCInstallDir redistributable folders. Missing required files fail the packaging step rather than
silently creating an incomplete distribution.

Only listed files are included. The supplied GAMEDATA/o2em-ng.db catalogue is included. No local configuration, ROMs, BIOS,
firmware, screenshots, PDB files or build logs are copied. The application uses the supplied catalogue and creates
its own configuration. Empty content folders include README files.
Each package has VERSION.txt and SHA256SUMS.txt.

A previous package/ZIP is preserved with a `.previous-<timestamp>` suffix.
Do not use the generated distribution as your permanent playing folder.
Debug builds are unaffected. To build Release without packaging, pass
`/p:SkipReleasePackaging=true` to MSBuild.

After packaging, run `Installer/Build Installer.bat` with Inno Setup 7 or 6 installed.
The installer reads `dist/release-info.iss`, generated from the same manifest version,
and consumes the prepared package. It does not copy your development configuration.
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
