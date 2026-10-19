# Installs the TARDIS as its own add-on vehicle, "tardis", so no game vehicle is replaced. Through OpenRPF:
#   mods\update\x64\dlcpacks\tardis\dlc.rpf  the pack: tools\dlc\*.xml/.meta + x64\levels\gta5\vehicles\tardis.rpf
#   mods\update\update.rpf                   a copy of the game's, with dlcpacks:/tardis/ added to dlclist.xml
# Game files are copied, never edited. Uses CodeWalker.Core (same calls as CodeWalker's RPF Explorer).
# A game update replaces update\update.rpf, so run this again after one.
# Usage: powershell -File tools\install_model.ps1 <CodeWalker folder> <GTA folder> [folder with gen9 maverick* files]
# The gen9 files default to assets\tardisv-gen9 in this repo.
param([string]$CodeWalker, [string]$Gta, [string]$Files = (Join-Path $PSScriptRoot '..\assets\tardisv-gen9'))
$ErrorActionPreference = 'Stop'
$dlcSrc = Join-Path $PSScriptRoot 'dlc'
Set-Location $CodeWalker
Add-Type -Path (Join-Path $CodeWalker 'CodeWalker.Core.dll')
[CodeWalker.GameFiles.GTA5Keys]::LoadFromPath($Gta, $true, $null)
[CodeWalker.GameFiles.RpfManager]::IsGen9 = $true
$Rpf = [CodeWalker.GameFiles.RpfFile]
$open = [CodeWalker.GameFiles.RpfEncryption]::OPEN
function Add($dir, $name, [byte[]]$bytes) { $Rpf::CreateFile($dir, $name, $bytes, $true) | Out-Null; "  $name" }
function SubDir($dir, [string]$path) { foreach ($p in $path.Split('\')) { $dir = $Rpf::CreateDirectory($dir, $p) }; $dir }

$mods = Join-Path $Gta 'mods'
# the TARDIS used to replace the Maverick in this copy; nothing else goes in it
$old = Join-Path $mods 'x64e.rpf'
if (Test-Path $old) { Remove-Item $old; "removed the old Maverick replacement (mods\x64e.rpf)" }

# the pack, rebuilt from scratch each time
$packDir = Join-Path $mods 'update\x64\dlcpacks\tardis'
New-Item -ItemType Directory -Force $packDir | Out-Null
Remove-Item (Join-Path $packDir 'dlc.rpf') -ErrorAction SilentlyContinue
"dlc.rpf"
$dlc = $Rpf::CreateNew($Gta, 'mods\update\x64\dlcpacks\tardis\dlc.rpf', $open)
foreach ($f in 'content.xml', 'setup2.xml') { Add $dlc.Root $f ([IO.File]::ReadAllBytes((Join-Path $dlcSrc $f))) }
$data = SubDir $dlc.Root 'common\data'
Add $data 'carvariations.meta' ([IO.File]::ReadAllBytes((Join-Path $dlcSrc 'carvariations.meta')))
Add (SubDir $data 'levels\gta5') 'vehicles.meta' ([IO.File]::ReadAllBytes((Join-Path $dlcSrc 'vehicles.meta')))
$veh = $Rpf::CreateNew((SubDir $dlc.Root 'x64\levels\gta5\vehicles'), 'tardis.rpf', $open)
foreach ($f in Get-ChildItem $Files -File -Filter 'maverick*') {
    Add $veh.Root ($f.Name -replace '^maverick', 'tardis') ([IO.File]::ReadAllBytes($f.FullName))
}

# dlclist.xml lives in update.rpf; the mods copy shadows the game's
$upd = Join-Path $mods 'update\update.rpf'
if (-not (Test-Path $upd)) {
    "copying update.rpf into mods (3 GB)..."
    Copy-Item (Join-Path $Gta 'update\update.rpf') $upd
}
$rpf = New-Object CodeWalker.GameFiles.RpfFile($upd, 'mods\update\update.rpf')
$rpf.ScanStructure([Action[string]]{ param($s) }, [Action[string]]{ param($s) Write-Host "ERR $s" })
$entry = $rpf.AllEntries | Where-Object { $_.Path -eq 'mods\update\update.rpf\common\data\dlclist.xml' }
if (-not $entry) { throw 'dlclist.xml not found in update.rpf' }
$xml = [Text.Encoding]::UTF8.GetString($rpf.ExtractFile($entry))
if ($xml -notmatch 'dlcpacks:/tardis/') {
    $Rpf::EnsureValidEncryption($rpf, [Func[CodeWalker.GameFiles.RpfFile, bool]]{ param($f) $true }) | Out-Null
    $xml = $xml -replace '(\s*)</Paths>', "`$1`t<Item>dlcpacks:/tardis/</Item>`$1</Paths>"
    Add $entry.Parent 'dlclist.xml' ([Text.Encoding]::UTF8.GetBytes($xml))
}
"dlclist.xml has dlcpacks:/tardis/"
