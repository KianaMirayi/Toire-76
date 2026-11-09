# TOiRE-76 build helpers (VS 2026 + WebView2)
# 用法: powershell -ExecutionPolicy Bypass -File scripts\build.ps1 [-Config Debug] [-Target TOIRE76_Standalone]
param(
    [ValidateSet("Debug", "Release", "RelWithDebInfo")]
    [string]$Config = "Debug",
    [string[]]$Target = @("TOIRE76_Standalone")
)

$ErrorActionPreference = "Stop"
$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

# 代理会让 MSBuild 报 MSB6001，构建前一律清掉
foreach ($v in @("HTTP_PROXY", "HTTPS_PROXY", "ALL_PROXY", "NO_PROXY",
                 "http_proxy", "https_proxy", "all_proxy", "no_proxy")) {
    Remove-Item "Env:$v" -ErrorAction SilentlyContinue
}

$cmake = "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"
if (-not (Test-Path $cmake)) { $cmake = "cmake" }

Write-Host "== cmake build ($Config / $Target) ==" -ForegroundColor Cyan
& $cmake --build "$root\build" --config $Config --target @Target -- /m /nologo /v:m
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

# 构建后同步 WebUI（改了前端不用重编也能生效）
$src = Join-Path $root "WebUI"
$artefacts = Join-Path $root "build\TOIRE76_artefacts\$Config"
$targets = @(
    (Join-Path $artefacts "Standalone\webui"),
    (Join-Path $artefacts "VST3\TOiRE-76.vst3\Contents\webui")
)
foreach ($t in $targets) {
    if (Test-Path $t) { Remove-Item $t -Recurse -Force }
    if (Test-Path (Split-Path $t -Parent)) {
        Copy-Item $src $t -Recurse -Force
        Write-Host "webui -> $t" -ForegroundColor DarkGray
    }
}
Write-Host "done: $artefacts" -ForegroundColor Green
