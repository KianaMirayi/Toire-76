# =============================================================================
#  build-installer.ps1 -- stage the payload and compile the Inno Setup installer
#
#  ASCII-only on purpose (Windows PowerShell 5.1 reads BOM-less UTF-8 as ANSI).
#
#  What it does
#    1. copies the RELEASE VST3 bundle into installer\payload\TOIRE-76.vst3;
#       the bundle already carries Contents\webui\ so nothing extra is staged;
#    2. refuses to continue if any required payload file is missing;
#    3. runs ISCC.exe on TOIRE-76.iss, writing ..\dist\TOIRE-76-<version>-win64.exe;
#    4. prints a SHA256 of the installer.
#
#  Usage:
#    powershell -NoProfile -ExecutionPolicy Bypass -File installer\build-installer.ps1
#    ... -Version 0.1.0
# =============================================================================

[CmdletBinding()]
param(
    [string]$Version = '0.1.0',
    [string]$Iscc    = "$env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
)

$ErrorActionPreference = 'Stop'

$here = Split-Path -Parent $MyInvocation.MyCommand.Path
$proj = Split-Path -Parent $here

$bundleSrc     = Join-Path $proj 'build\TOIRE76_artefacts\Release\VST3\TOIRE-76.vst3'
$payload       = Join-Path $here 'payload'
$payloadBundle = Join-Path $payload 'TOIRE-76.vst3'

Write-Host ('Project  : ' + $proj)
Write-Host ('Bundle   : ' + $bundleSrc)
Write-Host ('Version  : ' + $Version)

if (-not (Test-Path -LiteralPath $bundleSrc)) {
    throw ('Missing input: ' + $bundleSrc + '  (build Release first)')
}
if (-not (Test-Path -LiteralPath $Iscc)) {
    throw ('Missing ISCC: ' + $Iscc)
}

# --- 1. stage the payload ----------------------------------------------------
if (Test-Path -LiteralPath $payload) { Remove-Item -LiteralPath $payload -Recurse -Force }
New-Item -ItemType Directory -Path $payloadBundle -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $bundleSrc 'Contents') -Destination $payloadBundle -Recurse -Force

Get-ChildItem -LiteralPath $payloadBundle -Recurse -File |
    Where-Object { $_.Name -match '\.(bak|before-scale|orig)$' } |
    ForEach-Object { Write-Host ('  dropping stale: ' + $_.Name); Remove-Item -LiteralPath $_.FullName -Force }

# --- 2. payload completeness -------------------------------------------------
$required = @(
    'Contents\x86_64-win\TOIRE-76.vst3',
    'Contents\x86_64-win\WebView2Loader.dll',
    'Contents\webui\index.html',
    'Contents\webui\app.js',
    'Contents\webui\style.css',
    'Contents\webui\fonts\kings-latin.woff2'
)
foreach ($rel in $required) {
    $full = Join-Path $payloadBundle $rel
    if (-not (Test-Path -LiteralPath $full)) { throw ('Payload incomplete, missing: ' + $rel) }
    Write-Host ('  payload OK: ' + $rel + '  (' + (Get-Item -LiteralPath $full).Length + ' bytes)')
}

# --- 3. dependencies ---------------------------------------------------------
foreach ($d in @('deps\MicrosoftEdgeWebview2Setup.exe', 'deps\ChineseSimplified.isl')) {
    if (-not (Test-Path -LiteralPath (Join-Path $here $d))) {
        throw ('Missing dependency: ' + $d + ' (copy it from the TOIRE-2A installer\deps folder)')
    }
}

# --- 4. compile --------------------------------------------------------------
$iss = Join-Path $here 'TOIRE-76.iss'
Write-Host ('ISCC     : ' + $Iscc)

& $Iscc "/DAppVersion=$Version" $iss
if ($LASTEXITCODE -ne 0) { throw ('ISCC failed with exit code ' + $LASTEXITCODE) }

# --- 5. report ---------------------------------------------------------------
$out = Join-Path $proj ('dist\TOIRE-76-' + $Version + '-win64.exe')
if (-not (Test-Path -LiteralPath $out)) { throw ('Installer not found: ' + $out) }

$hash = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash
Write-Host ''
Write-Host ('Installer: ' + $out)
Write-Host ('Size     : ' + [math]::Round((Get-Item -LiteralPath $out).Length / 1MB, 2) + ' MB')
Write-Host ('SHA256   : ' + $hash)
