param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug')
. "$PSScriptRoot\toolchain.ps1"
$ctest = Join-Path (Split-Path -Parent $CMake) 'ctest.exe'
Assert-Tool $ctest
Invoke-Checked $ctest @('--test-dir', (Join-Path $ProjectRoot "build\native-$Configuration"), '--output-on-failure')
