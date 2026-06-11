# Pixora Windows 打包脚本:Release 构建 → windeployqt → 便携 zip
# 用法(Developer PowerShell,或自行先调 vcvars64):
#   .\packaging\windows\build-package.ps1 -QtDir E:\Dev\Qt\6.11.1\msvc2022_64
param(
    [string]$QtDir = "E:\Dev\Qt\6.11.1\msvc2022_64",
    [string]$Preset = "dev-release"
)
$ErrorActionPreference = "Stop"
$root = Resolve-Path "$PSScriptRoot\..\.."
$buildDir = "$root\build\$Preset"
$version = (Select-String -Path "$root\CMakeLists.txt" -Pattern 'project\(Pixora VERSION ([0-9.]+)').Matches[0].Groups[1].Value
$distName = "Pixora-$version-win64-portable"
$dist = "$root\build\dist\$distName"

cmake --preset $Preset
if ($LASTEXITCODE) { exit 1 }
cmake --build --preset $Preset
if ($LASTEXITCODE) { exit 1 }
ctest --preset $Preset
if ($LASTEXITCODE) { exit 1 }

if (Test-Path $dist) { Remove-Item $dist -Recurse -Force }
New-Item -ItemType Directory -Force $dist | Out-Null

Copy-Item "$buildDir\pixora.exe" $dist
# 三方 DLL:优先取 vcpkg applocal 放在 exe 旁的;不足则回退 vcpkg bin 全量
$thirdParty = @(Get-ChildItem "$buildDir\*.dll" -ErrorAction SilentlyContinue)
if ($thirdParty.Count -lt 3) {
    $thirdParty = @(Get-ChildItem "$buildDir\vcpkg_installed\x64-windows\bin\*.dll")
}
if ($thirdParty.Count -lt 3) { throw "third-party DLLs not found" }
$thirdParty | Copy-Item -Destination $dist
& "$QtDir\bin\windeployqt.exe" --release --no-translations --no-system-d3d-compiler `
    --no-opengl-sw "$dist\pixora.exe"
if ($LASTEXITCODE) { exit 1 }

$zip = "$root\build\dist\$distName.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $dist -DestinationPath $zip
Write-Output "PACKAGE OK: $zip"
