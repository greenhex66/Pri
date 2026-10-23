[Setup]
AppId={{D3F6A8B2-1C47-4E95-8A0D-7B5E2C9F4A61}
AppName=HEX-Q PRIMES
AppVersion=1.0.0
AppPublisher=HEX-Q
DefaultDirName={autopf}\HEX-Q PRIMES
DisableDirPage=yes
DisableProgramGroupPage=yes
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
OutputDir=..
OutputBaseFilename=HEX-Q-PRIMES-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Files]
Source: "..\dist\HEX-Q PRIMES.vst3\*"; DestDir: "{commoncf64}\VST3\HEX-Q PRIMES.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
