param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
. "$PSScriptRoot\toolchain.ps1"
Set-BuildEnvironment
Assert-Tool "$GccRoot\g++.exe"
$buildDir = Join-Path $ProjectRoot "build\native-$Configuration"
Invoke-Checked $CMake @('-S', $ProjectRoot, '-B', $buildDir, '-G', 'Ninja',
    "-DCMAKE_BUILD_TYPE=$Configuration", "-DCMAKE_MAKE_PROGRAM=$Ninja",
    "-DCMAKE_C_COMPILER=$GccRoot\gcc.exe", "-DCMAKE_CXX_COMPILER=$GccRoot\g++.exe")
Invoke-Checked $CMake @('--build', $buildDir, '--parallel', '4')
