; Muslimtify Inno Setup Installer
; Requires Inno Setup 6+

#define MyAppName "Muslimtify"
#ifndef MyAppVersion
  #define MyAppVersion "0.2.3"
#endif
#ifndef Arch
  #define Arch "x64"
#endif
#define MyAppPublisher "rizukirr"
#define MyAppURL "https://github.com/rizukirr/muslimtify"
#define MyAppExeName "muslimtify.exe"
#define StagingDir "..\..\installer\staging"

[Setup]
AppId={{E8A3B2C1-4D5F-6A7B-8C9D-0E1F2A3B4C5D}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={localappdata}\Programs\{#MyAppName}
DefaultGroupName={#MyAppName}
PrivilegesRequired=lowest
OutputDir=..\..\dist
OutputBaseFilename=muslimtify-{#MyAppVersion}-setup-{#Arch}
SetupIconFile=..\..\assets\muslimtify.ico
UninstallDisplayIcon={app}\share\icons\muslimtify.ico
Compression=lzma2
SolidCompression=yes
ChangesEnvironment=yes
WizardStyle=modern
MinVersion=10.0
#if Arch == "arm64"
ArchitecturesAllowed=arm64
ArchitecturesInstallIn64BitMode=arm64
#else
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
#endif

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#StagingDir}\bin\muslimtify.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#StagingDir}\bin\muslimtify-service.exe"; DestDir: "{app}\bin"; Flags: ignoreversion
Source: "{#StagingDir}\bin\*.dll"; DestDir: "{app}\bin"; Flags: ignoreversion skipifsourcedoesntexist
Source: "{#StagingDir}\share\icons\muslimtify.ico"; DestDir: "{app}\share\icons"; Flags: ignoreversion

[Icons]
Name: "{userstartmenu}\{#MyAppName}"; Filename: "{app}\bin\{#MyAppExeName}"; \
  IconFilename: "{app}\share\icons\muslimtify.ico"; \
  AppUserModelID: "{#MyAppName}"

[Registry]
Root: HKCU; Subkey: "Environment"; ValueType: expandsz; ValueName: "Path"; \
  ValueData: "{olddata};{app}\bin"; Check: NeedsAddPath(ExpandConstant('{app}\bin'))

[Run]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "daemon install"; \
  StatusMsg: "Registering scheduled task..."; Flags: runhidden waituntilterminated

[UninstallRun]
Filename: "{app}\bin\{#MyAppExeName}"; Parameters: "daemon uninstall"; \
  StatusMsg: "Removing scheduled task..."; Flags: runhidden waituntilterminated; \
  RunOnceId: "RemoveScheduledTask"

[UninstallDelete]
Type: filesandordirs; Name: "{localappdata}\muslimtify"

[Code]
function NeedsAddPath(Param: string): Boolean;
var
  OrigPath: string;
  ParamExpanded: string;
begin
  if not RegQueryStringValue(HKCU, 'Environment', 'Path', OrigPath) then
  begin
    Result := True;
    exit;
  end;

  ParamExpanded := Param;
  Result := Pos(';' + Uppercase(ParamExpanded) + ';', ';' + Uppercase(OrigPath) + ';') = 0;
end;

procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  OrigPath: string;
  AppBinDir: string;
  NewPath: string;
  P: Integer;
begin
  if CurUninstallStep = usPostUninstall then
  begin
    if RegQueryStringValue(HKCU, 'Environment', 'Path', OrigPath) then
    begin
      AppBinDir := ExpandConstant('{app}\bin');
      P := Pos(';' + Uppercase(AppBinDir), ';' + Uppercase(OrigPath));
      if P > 0 then
      begin
        NewPath := Copy(OrigPath, 1, P - 1) + Copy(OrigPath, P + Length(AppBinDir) + 1, MaxInt);
        if (Length(NewPath) > 0) and (NewPath[1] = ';') then
          NewPath := Copy(NewPath, 2, MaxInt);
        while Pos(';;', NewPath) > 0 do
          StringChangeEx(NewPath, ';;', ';', True);
        RegWriteStringValue(HKCU, 'Environment', 'Path', NewPath);
      end;
    end;
  end;
end;
