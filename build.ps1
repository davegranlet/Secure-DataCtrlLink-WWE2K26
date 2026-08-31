$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path

# Prefer an explicit LLVM-MinGW installation, then fall back to the caller's PATH.
# This keeps the public build reproducible without embedding a developer's PC path.
$Compiler = if ($env:LLVM_MINGW_ROOT) {
    Join-Path $env:LLVM_MINGW_ROOT 'bin\clang++.exe'
} else {
    (Get-Command 'clang++.exe' -ErrorAction Stop).Source
}
$CCompiler = if ($env:LLVM_MINGW_ROOT) {
    Join-Path $env:LLVM_MINGW_ROOT 'bin\clang.exe'
} else {
    (Get-Command 'clang.exe' -ErrorAction Stop).Source
}
if (-not (Test-Path -LiteralPath $Compiler) -or -not (Test-Path -LiteralPath $CCompiler)) {
    throw 'LLVM-MinGW was not found. Put clang/clang++ on PATH or set LLVM_MINGW_ROOT.'
}
$BuildDir = Join-Path $ProjectRoot 'build'
$SourceDir = Join-Path $ProjectRoot 'src'
$MinHookDir = Join-Path $ProjectRoot 'third_party\minhook'
New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null

$Common = @('-std=c++20', '-O2', '-Wall', '-Wextra', '-Wpedantic', '-Werror', '-I', $SourceDir, '-I', (Join-Path $MinHookDir 'include'))
$MinHookSources = @('buffer.c', 'hook.c', 'trampoline.c', 'hde\hde64.c')
$MinHookObjects = @()
foreach ($Source in $MinHookSources) {
    $Object = Join-Path $BuildDir (('minhook-' + $Source.Replace('\', '-').Replace('.c', '.o')))
    & $CCompiler '-std=c17' '-O2' '-Wall' '-Wextra' '-Werror' '-D_WIN64' '-I' (Join-Path $MinHookDir 'include') '-I' (Join-Path $MinHookDir 'src') '-I' (Join-Path $MinHookDir 'src\hde') '-c' (Join-Path $MinHookDir ('src\' + $Source)) '-o' $Object
    if ($LASTEXITCODE -ne 0) { throw "MinHook build failed: $Source" }
    $MinHookObjects += $Object
}
& $Compiler @Common '-static' '-static-libgcc' '-static-libstdc++' (Join-Path $SourceDir 'bank_builder.cpp') (Join-Path $ProjectRoot 'tests\bank_builder_tests.cpp') '-o' (Join-Path $BuildDir 'bank_builder_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Test build failed' }

& $Compiler @Common '-municode' '-static' '-static-libgcc' '-static-libstdc++' (Join-Path $ProjectRoot 'tests\proxy_smoke.cpp') '-o' (Join-Path $BuildDir 'proxy_smoke.exe')
if ($LASTEXITCODE -ne 0) { throw 'Proxy smoke-test build failed' }

& $Compiler @Common '-static' '-static-libgcc' '-static-libstdc++' (Join-Path $SourceDir 'status_message.cpp') (Join-Path $ProjectRoot 'tests\status_message_tests.cpp') '-o' (Join-Path $BuildDir 'status_message_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'Status-message test build failed' }

& $Compiler @Common '-municode' '-static' '-static-libgcc' '-static-libstdc++' (Join-Path $SourceDir 'cak_validator.cpp') (Join-Path $ProjectRoot 'tests\cak_validator_tests.cpp') '-o' (Join-Path $BuildDir 'cak_validator_tests.exe')
if ($LASTEXITCODE -ne 0) { throw 'CAK validator test build failed' }

& $Compiler @Common '-shared' '-static' '-static-libgcc' '-static-libstdc++' (Join-Path $SourceDir 'bank_builder.cpp') (Join-Path $SourceDir 'cak_validator.cpp') (Join-Path $SourceDir 'status_message.cpp') (Join-Path $SourceDir 'proxy.cpp') @MinHookObjects (Join-Path $ProjectRoot 'exports.def') '-lbcrypt' '-luser32' '-o' (Join-Path $BuildDir 'dinput8.dll')
if ($LASTEXITCODE -ne 0) { throw 'DLL build failed' }

Write-Host "Built artifacts in $BuildDir"
