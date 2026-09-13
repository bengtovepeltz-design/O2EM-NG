#define MyAppName "O2EM-NG"
#include "..\dist\release-info.iss"
#define MyAppPublisher "Bengt-Ove Peltz"
#define MyAppExeName "O2EM-NG.exe"

[Setup]
AppId={{E8C65F7D-8F6E-4A4A-A271-BA3BBDBA67C1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={localappdata}\Programs\O2EM-NG
; Keep upgrades from reusing the old, protected Program Files location.
UsePreviousAppDir=no
DefaultGroupName=O2EM-NG
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=O2EM-NG-v{#MyAppVersion}-Setup
SetupIconFile=..\assets\O2EM-NG.ico
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=lowest
UninstallDisplayIcon={app}\{#MyAppExeName}

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"
Name: "swedish"; MessagesFile: "compiler:Languages\Swedish.isl"

[Dirs]
Name: "{app}\ROMS"
Name: "{app}\BIOS"
Name: "{app}\MANUALS"
Name: "{app}\BOXART"
Name: "{app}\SCREENSHOTS"
Name: "{app}\CARTRIDGES"
Name: "{app}\Gamedata"

[Files]
; All distributable content comes from the validated package, never the working tree.
Source: "{#PackageDirectory}\*"; DestDir: "{app}"; Excludes: "GAMEDATA\o2em-ng.db"; Flags: ignoreversion recursesubdirs createallsubdirs
; Seed new installations without overwriting an existing user's catalogue.
Source: "{#PackageDirectory}\GAMEDATA\o2em-ng.db"; DestDir: "{app}\GAMEDATA"; Flags: onlyifdoesntexist uninsneveruninstall

[Icons]
Name: "{autoprograms}\O2EM-NG"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\O2EM-NG"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch O2EM-NG"; Flags: nowait postinstall skipifsilent
