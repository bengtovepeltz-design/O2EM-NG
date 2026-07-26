#define MyAppName "O2EM-NG"
#define MyAppVersion "Beta 3"
#define MyAppPublisher "Bengt-Ove Peltz"
#define MyAppExeName "O2EM-NG.exe"

[Setup]
AppId={{E8C65F7D-8F6E-4A4A-A271-BA3BBDBA67C1}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\O2EM-NG
DefaultGroupName=O2EM-NG
DisableProgramGroupPage=yes
OutputDir=Output
OutputBaseFilename=O2EM-NG-Beta3-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
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
Source: "..\x64\Release\O2EM-NG.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\x64\Release\*.dll"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs skipifsourcedoesntexist
Source: "..\third_party\pdfium\pdfium.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist
Source: "..\assets\*"; DestDir: "{app}\assets"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\Gamedata\o2em-ng.db"; DestDir: "{app}\Gamedata"; Flags: ignoreversion
Source: "..\Gamedata\Gamelist.txt"; DestDir: "{app}\Gamedata"; Flags: ignoreversion
Source: "..\o2em-ng.cfg"; DestDir: "{app}"; Flags: onlyifdoesntexist
Source: "..\README_SV.txt"; DestDir: "{app}"; Flags: ignoreversion
Source: "..\Docs\COPYRIGHTS.txt"; DestDir: "{app}\Docs"; Flags: ignoreversion

[Icons]
Name: "{autoprograms}\O2EM-NG"; Filename: "{app}\{#MyAppExeName}"
Name: "{autodesktop}\O2EM-NG"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Tasks]
Name: "desktopicon"; Description: "Create a &desktop shortcut"; GroupDescription: "Additional shortcuts:"; Flags: unchecked

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "Launch O2EM-NG"; Flags: nowait postinstall skipifsilent
