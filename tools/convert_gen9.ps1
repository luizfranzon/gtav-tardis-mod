# Converts Legacy GTA V assets (.yft/.ytd/.ydr/.ydd) to Gen9 (Enhanced) with CodeWalker's own converter
# (the same calls as CodeWalker's Tools > Asset Converter). Runs on Windows PowerShell 5.1 (.NET Framework).
# Usage: powershell -File tools\convert_gen9.ps1 <CodeWalker folder> <input folder> <output folder>
param([string]$CodeWalker, [string]$In, [string]$Out)
$ErrorActionPreference = 'Stop'
Set-Location $CodeWalker # ShadersGen9Conversion.xml is read from here
Add-Type -Path (Join-Path $CodeWalker 'CodeWalker.Core.dll')
[CodeWalker.GameFiles.RpfManager]::IsGen9 = $true
New-Item -ItemType Directory -Force $Out | Out-Null
foreach ($f in Get-ChildItem $In -File) {
    $type = @{ '.yft' = 'YftFile'; '.ytd' = 'YtdFile'; '.ydr' = 'YdrFile'; '.ydd' = 'YddFile' }[$f.Extension.ToLower()]
    if (-not $type) { continue }
    $file = New-Object "CodeWalker.GameFiles.$type"
    $file.Load([IO.File]::ReadAllBytes($f.FullName))
    [IO.File]::WriteAllBytes((Join-Path $Out $f.Name), $file.Save())
    "{0} -> gen9" -f $f.Name
}
