# Packs the release zip, laid out like the game folder so it can be extracted straight into it: build\TARDIS-<version>.zip
# Needs build\TARDIS.asi (build.bat) and the add-on pack that tools\install_model.ps1 built in the game folder.
# Usage: pwsh -File tools\package.ps1 [version] [GTA folder]
# PowerShell 7 (pwsh): 5.1's Compress-Archive writes backslashes into the zip paths, which some extractors choke on.
param([string]$Version = 'dev', [string]$Gta = 'E:\SteamLibrary\steamapps\common\Grand Theft Auto V Enhanced')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot
$stage = Join-Path $root 'build\release'
$pack = 'mods\update\x64\dlcpacks\tardis'
Remove-Item $stage -Recurse -Force -ErrorAction SilentlyContinue
foreach ($d in 'TARDIS\sounds', 'TARDIS\translations', $pack) { New-Item -ItemType Directory -Force (Join-Path $stage $d) | Out-Null }
Copy-Item (Join-Path $root 'build\TARDIS.asi'), (Join-Path $root 'TARDIS.ini') $stage
Copy-Item (Join-Path $root 'assets\sounds\*.wav') (Join-Path $stage 'TARDIS\sounds')
Copy-Item (Join-Path $root 'translations\*.ini') (Join-Path $stage 'TARDIS\translations')
Copy-Item (Join-Path $Gta "$pack\dlc.rpf") (Join-Path $stage $pack)
$zip = Join-Path $root "build\TARDIS-$Version.zip"
Compress-Archive -Path (Join-Path $stage '*') -DestinationPath $zip -Force
"packed $zip"
