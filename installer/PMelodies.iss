; SPDX-License-Identifier: AGPL-3.0-or-later
; Copyright (C) 2026 JoseFrMunoz
; This file is part of P.Melodies. See LICENSE for the full license text.
;
; Windows installer (Inno Setup 6.3 or later). CI builds it with:
;   ISCC.exe /DAppVersion=0.1.0 /DVst3Bundle=<path>\P.Melodies.vst3 /DOutputDir=<dir> installer\PMelodies.iss
;
; What it does:
;   - Copies the VST3 bundle to C:\Program Files\Common Files\VST3\P.Melodies.vst3
;     (the standard VST3 folder that FL Studio scans).
;   - Puts the uninstaller and the licences in C:\Program Files\P.Melodies, so nothing but the
;     plugin lands in the VST3 folder.
;   - Registers an uninstaller in Windows Settings > Apps.
;   - Spanish or English, following the Windows language.

#ifndef AppVersion
  #define AppVersion "0.0.0"
#endif
#ifndef Vst3Bundle
  #define Vst3Bundle "..\build\PMelodies_artefacts\Release\VST3\P.Melodies.vst3"
#endif
#ifndef OutputDir
  #define OutputDir "..\build\installer"
#endif

[Setup]
; AppId identifies P.Melodies for upgrades and uninstall. NEVER change it.
AppId={{F203E279-8139-4823-879B-18EBF7782815}
AppName=P.Melodies
AppVersion={#AppVersion}
AppVerName=P.Melodies {#AppVersion}
AppPublisher=JoseFrMunoz
AppPublisherURL=https://github.com/JoseFrMunoz/P.Melodies
AppSupportURL=https://github.com/JoseFrMunoz/P.Melodies/issues
AppUpdatesURL=https://github.com/JoseFrMunoz/P.Melodies/releases
AppCopyright=Copyright (C) 2026 JoseFrMunoz
VersionInfoVersion={#AppVersion}
DefaultDirName={commonpf64}\P.Melodies
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=no
PrivilegesRequired=admin
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0
OutputDir={#OutputDir}
OutputBaseFilename=P.Melodies-{#AppVersion}-windows-setup
Compression=lzma2/max
SolidCompression=yes
WizardStyle=modern
ShowLanguageDialog=auto
; Offer to close FL Studio if it has the plugin loaded while installing or uninstalling.
CloseApplications=yes
RestartApplications=no
UninstallDisplayName=P.Melodies (VST3)

[Languages]
Name: "spanish"; MessagesFile: "compiler:Languages\Spanish.isl"; InfoBeforeFile: "info-es.txt"
Name: "english"; MessagesFile: "compiler:Default.isl"; InfoBeforeFile: "info-en.txt"

[Messages]
spanish.FinishedLabel=P.Melodies se ha instalado en C:\Program Files\Common Files\VST3.%n%nAbre FL Studio y reescanea los plugins: Options > Manage plugins > Find installed plugins.
english.FinishedLabel=P.Melodies has been installed to C:\Program Files\Common Files\VST3.%n%nOpen FL Studio and rescan your plugins: Options > Manage plugins > Find installed plugins.

[InstallDelete]
; Start from a clean bundle so no file from a previous version is left behind.
Type: filesandordirs; Name: "{commoncf64}\VST3\P.Melodies.vst3"

[Files]
Source: "{#Vst3Bundle}\*"; DestDir: "{commoncf64}\VST3\P.Melodies.vst3"; Flags: ignoreversion recursesubdirs createallsubdirs
Source: "..\LICENSE"; DestDir: "{app}"; DestName: "LICENSE.txt"; Flags: ignoreversion
Source: "..\licenses\*"; DestDir: "{app}\licenses"; Flags: ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\P.Melodies.vst3"
