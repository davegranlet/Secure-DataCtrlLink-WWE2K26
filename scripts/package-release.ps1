param(
    [Parameter(Mandatory = $false)]
    [string]$DllPath = '',

    [Parameter(Mandatory = $false)]
    [string]$Version = '1.0.0',

    [Parameter(Mandatory = $false)]
    [string]$OutputRoot = ''
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($DllPath)) {
    $DllPath = Join-Path $ProjectRoot 'build\dinput8.dll'
}
$DllPath = [IO.Path]::GetFullPath($DllPath)
if (-not (Test-Path -LiteralPath $DllPath -PathType Leaf)) {
    throw "Build the addon first or pass -DllPath. Missing: $DllPath"
}

$ProvenanceFaq = Join-Path $ProjectRoot 'docs\DEVELOPMENT-AND-PROVENANCE-FAQ.md'
$Methodology = Join-Path $ProjectRoot 'docs\METHODOLOGY-AND-PROVENANCE.md'
$ReleaseTemplate = Join-Path $ProjectRoot 'docs\RELEASE-NOTES-TEMPLATE.md'
foreach ($RequiredDocument in @($ProvenanceFaq, $Methodology, $ReleaseTemplate)) {
    if (-not (Test-Path -LiteralPath $RequiredDocument -PathType Leaf)) {
        throw "Required release documentation is missing: $RequiredDocument"
    }
}

$ReadabilityDocuments = @(
    Get-ChildItem -LiteralPath $ProjectRoot -Filter '*.md' -File |
        Where-Object { $_.Name -ne 'THIRD_PARTY_NOTICES.md' }
    Get-ChildItem -LiteralPath (Join-Path $ProjectRoot 'docs') -Filter '*.md' -File -Recurse
)
$RequiredReadabilityStatement = 'I ran this document through an “explain like I am five”'
foreach ($Document in $ReadabilityDocuments) {
    $DocumentPath = if ($Document -is [System.IO.FileInfo]) { $Document.FullName } else { [string]$Document }
    if (-not (Test-Path -LiteralPath $DocumentPath -PathType Leaf)) {
        throw "Required first-party documentation is missing: $DocumentPath"
    }

    $NormalizedDocument = (Get-Content -Raw -LiteralPath $DocumentPath) -replace '\s+', ' '
    if (-not $NormalizedDocument.Contains($RequiredReadabilityStatement)) {
        throw "Required readability notice is missing from: $DocumentPath"
    }
}

$FaqText = Get-Content -Raw -LiteralPath $ProvenanceFaq
$RequiredStatement = 'The project did not begin with a request for AI to invent or reproduce an addon.'
$NormalizedFaq = $FaqText -replace '\s+', ' '
if (-not $NormalizedFaq.Contains($RequiredStatement)) {
    throw 'The canonical development FAQ is missing the required project-origin statement.'
}

$ReleaseRoot = if (-not [string]::IsNullOrWhiteSpace($OutputRoot)) {
    [IO.Path]::GetFullPath($OutputRoot)
} else {
    Join-Path $ProjectRoot 'release'
}
$null = New-Item -ItemType Directory -Path $ReleaseRoot -Force
$Stage = Join-Path $ReleaseRoot "Secure-DataCtrlLink-WWE2K26-$Version"
$Zip = "$Stage.zip"
if (Test-Path -LiteralPath $Stage) {
    throw "Release stage folder already exists: $Stage`nDelete it manually, then re-run this script."
}
if (Test-Path -LiteralPath $Zip) {
    throw "Release ZIP already exists: $Zip`nDelete it manually, then re-run this script."
}
New-Item -ItemType Directory -Path $Stage -Force | Out-Null

Copy-Item -LiteralPath $DllPath -Destination (Join-Path $Stage 'dinput8.dll')
New-Item -ItemType Directory -Path (Join-Path $Stage 'plugins') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'README.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'LICENSE') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'SECURITY.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'THIRD_PARTY_NOTICES.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\EXPLAIN-LIKE-IM-FIVE.md') -Destination $Stage
Copy-Item -LiteralPath $ProvenanceFaq -Destination $Stage
Copy-Item -LiteralPath $Methodology -Destination $Stage
Copy-Item -LiteralPath $ReleaseTemplate -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\ADDON-MANAGEMENT.md') -Destination $Stage
Copy-Item -LiteralPath (Join-Path $ProjectRoot 'docs\BUGS-AND-FIXES-1.7.7.md') -Destination $Stage

$DllHash = (Get-FileHash -Algorithm SHA256 (Join-Path $Stage 'dinput8.dll')).Hash
Set-Content -LiteralPath (Join-Path $Stage 'SHA256SUMS.txt') -Encoding ascii -Value "$DllHash  dinput8.dll"
Compress-Archive -LiteralPath $Stage -DestinationPath $Zip -CompressionLevel Optimal
$ZipHash = (Get-FileHash -Algorithm SHA256 $Zip).Hash
Set-Content -LiteralPath "$Zip.sha256.txt" -Encoding ascii -Value "$ZipHash  $([IO.Path]::GetFileName($Zip))"

Write-Host "Created $Zip"
Write-Host "DLL SHA-256: $DllHash"
Write-Host "ZIP SHA-256: $ZipHash"
