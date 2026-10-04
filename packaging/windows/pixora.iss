; Pixora Inno Setup 安装脚本
; build-package.ps1 生成便携目录后,若检测到 Inno Setup 会自动以本脚本编译安装包;
; 手动编译:iscc /DAppVersion=<版本> packaging\windows\pixora.iss
; (需安装 Inno Setup 6: https://jrsoftware.org/isinfo.php)

#ifndef AppVersion
  #error 请以 /DAppVersion=x.y.z 传入版本号(build-package.ps1 从 CMakeLists.txt 读取并传入)
#endif
#define DistDir "..\..\build\dist\Pixora-" + AppVersion + "-win64-portable"

[Setup]
AppId={{8E1B0A9C-7D24-4C7E-9B0E-PIXORA000001}
AppName=Pixora
AppVersion={#AppVersion}
AppPublisher=Pixora
; 按当前用户安装(无需管理员):自启项、设置都在 HKCU / %APPDATA%,
; 若以管理员提权安装,"另一个管理员账号代为提权"时会写进那个账号的 HKCU
PrivilegesRequired=lowest
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

[Dirs]
Name: "{userappdata}\Pixora"; Flags: uninsneveruninstall

[Registry]
; 与程序内"开机自动启动"写入形态一致(带 --autostart 启动参数)
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Run"; \
    ValueType: string; ValueName: "Pixora"; ValueData: """{app}\pixora.exe"" --autostart"; \
    Tasks: autostart
; 清掉任务管理器里可能残留的"已禁用"记录,否则 Run 项写了也不生效
Root: HKCU; Subkey: "Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run"; \
    ValueType: none; ValueName: "Pixora"; Flags: deletevalue; Tasks: autostart

[INI]
; 同步程序侧的自启意图,使设置面板与启动对账认得安装器开启的自启
Filename: "{userappdata}\Pixora\Pixora.ini"; Section: "startup"; Key: "autoStart"; \
    String: "true"; Tasks: autostart

[Run]
Filename: "{app}\pixora.exe"; Description: "立即运行 Pixora"; \
    Flags: nowait postinstall skipifsilent

[Code]
const
  RunKey = 'Software\Microsoft\Windows\CurrentVersion\Run';
  ApprovedKey = 'Software\Microsoft\Windows\CurrentVersion\Explorer\StartupApproved\Run';

{ 卸载时清理指向本次安装的自启项——不论是安装时勾选的,还是之后在程序设置里
  开启的(后者不经安装器登记,靠 uninsdeletevalue 清不掉,会留下指向已删除
  exe 的启动项)。指向其它副本(如便携版)的自启项不动。 }
procedure CurUninstallStepChanged(CurUninstallStep: TUninstallStep);
var
  Command: String;
begin
  if CurUninstallStep = usUninstall then
  begin
    if RegQueryStringValue(HKCU, RunKey, 'Pixora', Command) and
       (Pos(Lowercase(ExpandConstant('{app}\pixora.exe')), Lowercase(Command)) > 0) then
    begin
      RegDeleteValue(HKCU, RunKey, 'Pixora');
      RegDeleteValue(HKCU, ApprovedKey, 'Pixora');
    end;
  end;
end;
