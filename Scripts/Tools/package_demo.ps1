<#
  Packs a playable demo of the game for another Windows PC.

  Usage (Unreal Editor closed, from the project root):
    powershell -ExecutionPolicy Bypass -File Scripts\Tools\package_demo.ps1
    powershell -ExecutionPolicy Bypass -File Scripts\Tools\package_demo.ps1 -Config Development -SkipCook -NoZip

  Shipping   = the demo for other PCs (no console, no log files).
  Development = same cooked content, but writes DerClou3\Saved\Logs next to the exe and accepts -DCSelfTest=...
               (for testing the packaged game here). -SkipCook reuses the last cook.

  Result: Dist\Lindenhof-Demo-<date>\ (ready to run) and Dist\Lindenhof-Demo-<date>.zip
  The readme for players lives in Scripts\Tools\demo_readme.txt (copied in as "Lies mich.txt").
#>
param(
    [ValidateSet('Shipping', 'Development')] [string]$Config = 'Shipping',
    [switch]$SkipCook,
    [switch]$SkipBuild,
    [switch]$NoZip
)

$ErrorActionPreference = 'Stop'
$Root = (Resolve-Path (Join-Path $PSScriptRoot '..\..')).Path
$Engine = 'C:\Program Files\Epic Games\UE_5.8'
$Uat = Join-Path $Engine 'Engine\Build\BatchFiles\RunUAT.bat'
$Project = Join-Path $Root 'DerClou3.uproject'
$Date = Get-Date -Format 'yyyy-MM-dd'
$Name = if ($Config -eq 'Shipping') { "Lindenhof-Demo-$Date" } else { "Lindenhof-Demo-$Date-Development" }
$Dist = Join-Path $Root 'Dist'
$WorkDir = Join-Path $Root 'Saved\Packaging'
$Archive = Join-Path $WorkDir $Config
$Log = Join-Path $WorkDir "package_$Config.log"

if (Get-Process -Name 'UnrealEditor', 'UnrealEditor-Cmd' -ErrorAction SilentlyContinue) {
    throw 'Unreal Editor is running - close it first (the build needs the editor DLLs).'
}
New-Item -ItemType Directory -Force -Path $WorkDir, $Dist | Out-Null

# 1. Build the game exe, cook the three maps (+ always-cook folders), stage, pak (IoStore), archive.
$UatArgs = @(
    'BuildCookRun', "-project=$Project", '-noP4', '-platform=Win64', "-clientconfig=$Config",
    '-stage', '-pak', '-iostore', '-compressed', '-prereqs', '-nodebuginfo',
    '-archive', "-archivedirectory=$Archive", '-utf8output', '-unattended'
)
if ($SkipBuild) { $UatArgs += '-skipbuild' } else { $UatArgs += '-build' }
if ($SkipCook) { $UatArgs += '-skipcook' } else { $UatArgs += '-cook' }

if (Test-Path $Archive) { Remove-Item -Recurse -Force $Archive }
Write-Host "RunUAT $($UatArgs -join ' ')"
Write-Host "Log: $Log"
# cmd /c keeps PowerShell 5.1 from turning the tool's stderr lines into errors.
cmd /c "`"$Uat`" $($UatArgs -join ' ') > `"$Log`" 2>&1"
if ($LASTEXITCODE -ne 0) {
    Get-Content $Log -Tail 40
    throw "BuildCookRun failed (exit code $LASTEXITCODE), see $Log"
}

$Built = Join-Path $Archive 'Windows'
if (-not (Test-Path (Join-Path $Built 'DerClou3.exe'))) { throw "No DerClou3.exe in $Built" }

# 2. Player folder: the archived build, the launcher renamed after the working title, the readme.
$Out = Join-Path $Dist $Name
if (Test-Path $Out) { Remove-Item -Recurse -Force $Out }
Copy-Item -Recurse $Built $Out
# The root exe only starts DerClou3\Binaries\Win64\DerClou3[-Win64-Shipping].exe; its own name does not matter.
Rename-Item (Join-Path $Out 'DerClou3.exe') 'Lindenhof.exe'
Copy-Item (Join-Path $PSScriptRoot 'demo_readme.txt') (Join-Path $Out 'Lies mich.txt')
# Fallback for graphics cards without DirectX 12 / Shader Model 6 (the cook has SM5 shaders too).
Set-Content -Encoding ASCII -Path (Join-Path $Out 'Lindenhof mit DirectX 11.bat') -Value @(
    '@echo off',
    'rem Starts the game with DirectX 11 - for older graphics cards.',
    'start "" "%~dp0Lindenhof.exe" -dx11'
)
Get-ChildItem $Out -Recurse -Include '*.pdb', 'Manifest_*.txt', 'vc_redist.arm64.exe' | Remove-Item -Force

$Size = (Get-ChildItem $Out -Recurse | Measure-Object -Property Length -Sum).Sum / 1MB
Write-Host ('Folder: {0} ({1:N0} MB)' -f $Out, $Size)

# 3. Zip (tar.exe from Windows handles files over 2 GB, Compress-Archive in PowerShell 5.1 does not).
if (-not $NoZip) {
    $Zip = Join-Path $Dist "$Name.zip"
    if (Test-Path $Zip) { Remove-Item -Force $Zip }
    Push-Location $Dist
    try {
        & "$env:SystemRoot\System32\tar.exe" -a -c -f "$Name.zip" $Name
        if ($LASTEXITCODE -ne 0) { throw "tar failed (exit code $LASTEXITCODE)" }
    }
    finally { Pop-Location }
    Write-Host ('Zip: {0} ({1:N0} MB)' -f $Zip, ((Get-Item $Zip).Length / 1MB))
}
