param(
    [string]$QtRoot = $env:QT_ROOT,
    [string]$BuildDir = "",
    [string]$Dist = "",
    [string]$Config = "Release"
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if (-not $QtRoot) {
    $QtRoot = "C:\Qt\6.12.0\msvc2022_64"
}
if (-not (Test-Path $QtRoot)) {
    throw "Qt not found at $QtRoot. Pass -QtRoot C:\Qt\6.12.0\msvc2022_64"
}
if (-not $BuildDir) { $BuildDir = Join-Path $Root "build-windows" }
if (-not $Dist) { $Dist = Join-Path $Root "dist" }

cmake -S $Root -B $BuildDir -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH="$QtRoot" -DCMAKE_BUILD_TYPE=$Config
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }

cmake --build $BuildDir --config $Config
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

$Exe = Join-Path $BuildDir "$Config\DayBook.exe"
if (-not (Test-Path $Exe)) {
    $Exe = Join-Path $BuildDir "DayBook.exe"
}
if (-not (Test-Path $Exe)) {
    throw "DayBook.exe not found below $BuildDir"
}

$AppDir = Join-Path $Dist "DayBook"
Remove-Item -Recurse -Force $AppDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $AppDir | Out-Null
Copy-Item -Path $Exe -Destination $AppDir

$WinDeployQt = Join-Path $QtRoot "bin\windeployqt.exe"
if (-not (Test-Path $WinDeployQt)) {
    throw "windeployqt.exe not found at $WinDeployQt"
}
& $WinDeployQt --release --qmldir (Join-Path $Root "qml") (Join-Path $AppDir "DayBook.exe")
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

$Zip = Join-Path $Dist "DayBook-1.0.0-windows-x64.zip"
Remove-Item -Force $Zip -ErrorAction SilentlyContinue
Compress-Archive -Path "$AppDir\*" -DestinationPath $Zip

Write-Host ""
Write-Host "App folder: $AppDir"
Write-Host "Zip:        $Zip"
