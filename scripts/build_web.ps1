. "$PSScriptRoot\toolchain.ps1"
Set-BuildEnvironment
$emsdkEnv = Join-Path $EmsdkRoot 'emsdk_env.ps1'
Assert-Tool $emsdkEnv
$env:EMSDK_QUIET = '1'
. $emsdkEnv
$buildDir = Join-Path $ProjectRoot 'build\web'
$toolchain = Join-Path $EmsdkRoot 'upstream\emscripten\cmake\Modules\Platform\Emscripten.cmake'
Invoke-Checked $CMake @('-S', $ProjectRoot, '-B', $buildDir, '-G', 'Ninja',
    '-DCMAKE_BUILD_TYPE=Release', "-DCMAKE_MAKE_PROGRAM=$Ninja",
    "-DCMAKE_TOOLCHAIN_FILE=$toolchain", '-DPLATFORM=Web')
Invoke-Checked $CMake @('--build', $buildDir, '--parallel', '4')
