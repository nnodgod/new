[Setup]
AppName=VowelSynth
AppVersion=1.0.0
AppPublisher=Me
DefaultDirName={commoncf64}\VST3\VowelSynth.vst3
DisableDirPage=yes
DisableProgramGroupPage=yes
Uninstallable=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=dist
OutputBaseFilename=VowelSynth-Setup
Compression=lzma
SolidCompression=yes

[Files]
Source: "build\VowelSynth_artefacts\Release\VST3\VowelSynth.vst3\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion createallsubdirs
