# Installs the XsiMcp add-on into a Softimage user folder, where Softimage loads it at
# startup (the alternative to connecting Softimage to this repository's Workgroup).
# -Version picks the user folder, e.g. Softimage_2015_R2-SP2; default: the newest one.
param([string]$Version)
$autodesk = Join-Path $env:USERPROFILE 'Autodesk'
if (-not $Version) {
    $Version = Get-ChildItem $autodesk -Directory -Filter 'Softimage_*' -ErrorAction SilentlyContinue |
        Sort-Object LastWriteTime | Select-Object -Last 1 -ExpandProperty Name
}
if (-not $Version) { throw "No Softimage user folder in $autodesk; start Softimage once first." }
$target = Join-Path $autodesk "$Version\Addons\XsiMcp"
$source = Join-Path $PSScriptRoot '..\Workgroup\Addons\XsiMcp'
New-Item -ItemType Directory -Force $target | Out-Null
# Copies the code only: an installed connection.json (the token) stays as it is.
Get-ChildItem $source -Recurse -File -Filter '*.py' | ForEach-Object {
    $relative = $_.FullName.Substring((Resolve-Path $source).Path.Length + 1)
    $destination = Join-Path $target $relative
    New-Item -ItemType Directory -Force (Split-Path $destination) | Out-Null
    Copy-Item $_.FullName $destination -Force
}
Write-Host "Installed XsiMcp into $target"
