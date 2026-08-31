param(
    [Parameter(Mandatory = $false)]
    [string]$DllPath = (Join-Path (Split-Path -Parent $PSScriptRoot) 'build\dinput8.dll'),

    [Parameter(Mandatory = $false)]
    [string]$Version = '1.0.0'
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
$DllPath = [IO.Path]::GetFullPath($DllPath)
if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Build the addon first or pass -DllPath. Missing: $DllPath"
}

$ReleaseRoot = Join-Path $ProjectRoot 'release'
$Stage = Join-Path $ReleaseRoot "Secure-DataCtrlLink-WWE2K26-$Version"
$Zip = "$Stage.zip"
if (Test-Path -LiteralPath $Stage) { Remove-Item -LiteralPath $Stage -Recurse -Force }
if (Test-Path -LiteralPath $Zip) { Remove-Item -LiteralPath $Zip -Force }
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

Copy-Item -LiteralPath $DllPath -Destination (Join-Path $Stage 'dinput8.dll')
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'README.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'LICENSE') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'SECURITY.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'THIRD_PARTY_NOTICES.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\EXPLAIN-LIKE-IM-FIVE.md') -Destination $Stage

$DllHash = (Get-FileHash -Algorithm SHA256 (Join-Path $Stage 'dinput8.dll')).Hash
Set-Content -LiteralPath (Join-Path $Stage 'SHA256SUMS.txt') -Encoding ascii -Value "$DllHash  dinput8.dll"
Compress-Archive -LiteralPath $Stage -DestinationPath $Zip -CompressionLevel Optimal
$ZipHash = (Get-FileHash -Algorithm SHA256 $Zip).Hash
Set-Content -LiteralPath "$Zip.sha256.txt" -Encoding ascii -Value "$ZipHash  $([IO.Path]::GetFileName($Zip))"

Write-Host "Created $Zip"
Write-Host "DLL SHA-256: $DllHash"
Write-Host "ZIP SHA-256: $ZipHash"

