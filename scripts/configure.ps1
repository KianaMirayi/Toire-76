# TOiRE-76 configure helper (VS 2026 generator, WebView2 ON by default)
param(
    [switch]$NoWebView2
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

foreach ($v in @("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY",
                 "http_proxy", "https_proxy", "all_proxy", "no_proxy")) {
    Remove-Item "Env:$v" -ErrorAction SilentlyContinue
}

$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }

$web = if ($NoWebView2) { "OFF" } else { "ON" }
Write-Host "== cmake configure (TOIRE76_WEBVIEW2=$web) ==" -ForegroundColor Cyan
& $cmake -S $root -B "$root\build" -G "Visual Studio 18 2026" -A x64 -DTOIRE76_WEBVIEW2=$web
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }
Write-Host "configured: $root\build" -ForegroundColor Green
