# Builds v271-office.oxt from the extension source tree.
# Usage:  powershell -ExecutionPolicy Bypass -File build_oxt.ps1
$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$src = Join-Path $root "extension"
$out = Join-Path $root "v271-office.oxt"

if (-not (Test-Path -LiteralPath (Join-Path $src "description.xml"))) {
    Write-Error "extension/description.xml not found; run from v271/office-extension/"
    exit 1
}

$tmp = Join-Path $env:TEMP ("v271-oxt-" + [guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Path $tmp | Out-Null

# Copy the extension content (without editor droppings and caches).
Get-ChildItem -LiteralPath $src -Force | Where-Object {
        $_.Name -notin @('.DS_Store', '__pycache__') } |
    ForEach-Object { Copy-Item -LiteralPath $_.FullName -Destination $tmp -Recurse -Force }
Get-ChildItem -LiteralPath $tmp -Recurse -Directory -Filter "__pycache__" |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Recurse -Force }
Get-ChildItem -LiteralPath $tmp -Recurse -Filter "*.pyc" |
    ForEach-Object { Remove-Item -LiteralPath $_.FullName -Force }

# Compress-Archive requires the .zip extension; .oxt is a plain zip container.
$tmpZip = "$out.zip"
if (Test-Path -LiteralPath $tmpZip) { Remove-Item -LiteralPath $tmpZip -Force }
if (Test-Path -LiteralPath $out) { Remove-Item -LiteralPath $out -Force }

Compress-Archive -Path (Join-Path $tmp "*") -DestinationPath $tmpZip -CompressionLevel Optimal
Move-Item -LiteralPath $tmpZip -Destination $out -Force

Remove-Item -LiteralPath $tmp -Recurse -Force

Write-Host "Built $out"