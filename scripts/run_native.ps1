param([ValidateSet('Debug','Release')][string]$Configuration = 'Debug', [switch]$Smoke)
. "$PSScriptRoot\toolchain.ps1"
$buildDir = Join-Path $ProjectRoot "build\native-$Configuration"
$executable = Join-Path $buildDir 'last_connection.exe'
Assert-Tool $executable
Push-Location $buildDir
try {
    $runArgs = @()
    if ($Smoke) { $runArgs += '--smoke' }
    Invoke-Checked $executable $runArgs
} finally { Pop-Location }
