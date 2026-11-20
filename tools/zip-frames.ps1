# Packs the demo frames (evidence/frames/*.png) into evidence/frames.zip.
# Uses only the built-in Compress-Archive cmdlet. Run from the repository root:
#   powershell -ExecutionPolicy Bypass -File tools/zip-frames.ps1
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$frames = Join-Path $root 'evidence\frames'
$zip = Join-Path $root 'evidence\frames.zip'

if (-not (Test-Path $frames)) {
    Write-Error "No frames found. Run the app with --demo first."
}
$files = Get-ChildItem $frames -Filter 'frame_*.png' | Sort-Object Name
if ($files.Count -eq 0) {
    Write-Error "evidence/frames is empty. Run the app with --demo first."
}
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $files.FullName -DestinationPath $zip -CompressionLevel Optimal
$size = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Write-Output "Packed $($files.Count) frames into $zip ($size MB)"
