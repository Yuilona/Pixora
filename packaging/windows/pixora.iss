; Pixora Inno Setup 安装脚本
; 先运行 build-package.ps1 生成 build\dist\Pixora-<版本>-win64-portable\,
; 再以本脚本编译:iscc packaging\windows\pixora.iss
; (需安装 Inno Setup 6: https://jrsoftware.org/isinfo.php)

#define AppVersion "0.2.0"
#define DistDir "..\..\build\dist\Pixora-" + AppVersion + "-win64-portable"

[Setup]
AppId={{8E1B0A9C-7D24-4C7E-9B0E-PIXORA000001}
AppName=Pixora
AppVersion={#AppVersion}
AppPublisher=Pixora
DefaultDirName={autopf}\Pixora
DefaultGroupName=Pixora
UninstallDisplayIcon={app}\pixora.exe
OutputDir=..\..\build\dist
OutputBaseFilename=Pixora-{#AppVersion}-win64-setup
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible

[Languages]
Name: "chinesesimplified"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "autostart"; Description: "开机自动启动 Pixora"; Flags: unchecked

[Files]
Source: "{#DistDir}\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Icons]
Name: "{group}\Pixora"; Filename: "{app}\pixora.exe"
Name: "{group}\卸载 Pixora"; Filename: "{uninstallexe}"

[Registry]
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "Pixora"; ValueData: """{app}\pixora.exe"""; \
    Tasks: autostart; Flags: uninsdeletevalue

[Run]
Filename: "{app}\pixora.exe"; Description: "立即运行 Pixora"; \
    Flags: nowait postinstall skipifsilent
