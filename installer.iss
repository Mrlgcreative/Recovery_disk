; ============================================================================
; HDD Password Recovery Tool — Inno Setup Script
; ============================================================================
; Genere un installeur Windows professionnel avec :
;   - CLI (hdd_unlock.exe)
;   - GUI (hdd_unlock_gui.exe)
;   - Raccourcis bureau / menu demarrer
;   - Desinstallation propre
; ============================================================================

#define MyAppName      "HDD Password Recovery Tool"
#define MyAppVersion   "1.0.0"
#define MyAppPublisher "HDD Recovery"
#define MyAppURL       ""
#define MyAppExeName   "hdd_unlock_gui.exe"
#define MyAppCliName   "hdd_unlock.exe"

[Setup]
AppId={{B7E3C4A1-2D5F-48A0-9E1C-3F6D8A7B5E20}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppVerName={#MyAppName} {#MyAppVersion}
AppPublisher={#MyAppPublisher}
DefaultDirName={autopf}\HDD_Recovery
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
LicenseFile=LICENSE.txt
OutputDir=installer_output
OutputBaseFilename=HDD_Recovery_Setup_{#MyAppVersion}
SetupIconFile=resources\app.ico
UninstallDisplayIcon={app}\hdd_unlock_gui.exe
Compression=lzma2/ultra64
SolidCompression=yes
WizardStyle=modern
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
MinVersion=10.0
WizardImageFile=resources\wizard_large.bmp
WizardSmallImageFile=resources\wizard_small.bmp

[Languages]
Name: "french"; MessagesFile: "compiler:Languages\French.isl"
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked
Name: "addtopath"; Description: "Ajouter au PATH systeme (CLI)"; GroupDescription: "Options avancees :"; Flags: unchecked

[Files]
; Executables principaux
Source: "build\hdd_unlock_gui.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "build\hdd_unlock.exe"; DestDir: "{app}"; Flags: ignoreversion

; Documentation
Source: "README.md"; DestDir: "{app}\doc"; Flags: ignoreversion
Source: "LICENSE.txt"; DestDir: "{app}\doc"; Flags: ignoreversion

; DLLs MinGW runtime (si necessaire)
Source: "build\*.dll"; DestDir: "{app}"; Flags: ignoreversion skipifsourcedoesntexist

[Icons]
Name: "{group}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Comment: "Lancer l'interface graphique"
Name: "{group}\{#MyAppName} (CLI)"; Filename: "{app}\{#MyAppCliName}"; Comment: "Lancer en ligne de commande"
Name: "{group}\Documentation"; Filename: "{app}\doc\README.md"
Name: "{group}\{cm:UninstallProgram,{#MyAppName}}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\{#MyAppName}"; Filename: "{app}\{#MyAppExeName}"; Tasks: desktopicon

[Run]
Filename: "{app}\{#MyAppExeName}"; Description: "{cm:LaunchProgram,{#MyAppName}}"; Flags: nowait postinstall skipifsilent

[Registry]
; Ajouter au PATH si selectionne
Root: HKLM; Subkey: "SYSTEM\CurrentControlSet\Control\Session Manager\Environment"; ValueType: expandsz; ValueName: "Path"; ValueData: "{olddata};{app}"; Tasks: addtopath; Check: NeedsAddPath(ExpandConstant('{app}'))

[Code]
// Verifie si le chemin est deja dans le PATH
function NeedsAddPath(Param: string): boolean;
var
  OrigPath: string;
begin
  if not RegQueryStringValue(HKEY_LOCAL_MACHINE,
    'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
    'Path', OrigPath)
  then begin
    Result := True;
    exit;
  end;
  Result := Pos(';' + Uppercase(Param) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

// Retirer du PATH a la desinstallation
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  OrigPath, AppDir: string;
  P: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    AppDir := ExpandConstant('{app}');
    if RegQueryStringValue(HKEY_LOCAL_MACHINE,
      'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
      'Path', OrigPath) then
    begin
      P := Pos(';' + Uppercase(AppDir), ';' + Uppercase(OrigPath));
      if P > 0 then
      begin
        Delete(OrigPath, P - 1, Length(AppDir) + 1);
        RegWriteStringValue(HKEY_LOCAL_MACHINE,
          'SYSTEM\CurrentControlSet\Control\Session Manager\Environment',
          'Path', OrigPath);
      end;
    end;
  end;
end;
