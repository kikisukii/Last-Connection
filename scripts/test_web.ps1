. "$PSScriptRoot\toolchain.ps1"
Set-BuildEnvironment
$env:EMSDK_QUIET = '1'
. (Join-Path $EmsdkRoot 'emsdk_env.ps1')
$buildDir = Join-Path $ProjectRoot 'build/wasm-tests'
New-Item -ItemType Directory -Force -Path $buildDir | Out-Null
$sources = @(Get-ChildItem (Join-Path $ProjectRoot 'src/gameplay/*.cpp') | ForEach-Object { $_.FullName })
foreach ($name in @('movement_feel','traffic_service','time_attack')) {
    $output = Join-Path $buildDir "$name.js"
    # Large test main functions retain multiple traffic/RNG fixtures on stack.
    # This stack size is test-only; the playable application's link is unchanged.
    $arguments = @('-std=c++20','-O2','-fexceptions',"-I$ProjectRoot/include",
        "$ProjectRoot/tests/${name}_tests.cpp") + $sources + @('-sENVIRONMENT=node',
        '-sEXIT_RUNTIME=1','-sALLOW_MEMORY_GROWTH=1','-sSTACK_SIZE=1048576',
        '-sASSERTIONS=1','-sSTACK_OVERFLOW_CHECK=2','-o',$output)
    Invoke-Checked (Join-Path $EmsdkRoot 'upstream/emscripten/em++.bat') $arguments
    Invoke-Checked $env:EMSDK_NODE @($output)
}
