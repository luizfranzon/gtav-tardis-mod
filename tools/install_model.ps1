# Puts Gen9 vehicle files into mods\x64e.rpf\levels\gta5\vehicles.rpf for OpenRPF, replacing the game's ones there.
# The game's own x64e.rpf is copied, never edited. Uses CodeWalker.Core (same calls as CodeWalker's RPF Explorer).
# Usage: powershell -File tools\install_model.ps1 <CodeWalker folder> <GTA folder> <folder with gen9 files>
param([string]$CodeWalker, [string]$Gta, [string]$Files)
$ErrorActionPreference = 'Stop'
Set-Location $CodeWalker
Add-Type -Path (Join-Path $CodeWalker 'CodeWalker.Core.dll')
[CodeWalker.GameFiles.GTA5Keys]::LoadFromPath($Gta, $true, $null)
[CodeWalker.GameFiles.RpfManager]::IsGen9 = $true

$mods = Join-Path $Gta 'mods'
$modRpf = Join-Path $mods 'x64e.rpf'
New-Item -ItemType Directory -Force $mods | Out-Null
if (-not (Test-Path $modRpf)) {
    "copying x64e.rpf into mods (2 GB)..."
    Copy-Item (Join-Path $Gta 'x64e.rpf') $modRpf
}

$rpf = New-Object CodeWalker.GameFiles.RpfFile($modRpf, 'mods\x64e.rpf')
$rpf.ScanStructure([Action[string]]{ param($s) }, [Action[string]]{ param($s) Write-Host "ERR $s" })
$veh = $rpf.Children | Where-Object { $_.Path -eq 'mods\x64e.rpf\levels\gta5\vehicles.rpf' }
if (-not $veh) { throw 'vehicles.rpf not found in x64e.rpf' }
[CodeWalker.GameFiles.RpfFile]::EnsureValidEncryption($veh, [Func[CodeWalker.GameFiles.RpfFile, bool]]{ param($f) $true }) | Out-Null

foreach ($f in Get-ChildItem $Files -File) {
    [CodeWalker.GameFiles.RpfFile]::CreateFile($veh.Root, $f.Name, [IO.File]::ReadAllBytes($f.FullName), $true) | Out-Null
    "replaced $($f.Name)"
}
