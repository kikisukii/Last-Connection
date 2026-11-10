$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $PSScriptRoot
function Find-CommandPath([string]$Name) {
    $command = Get-Command $Name -ErrorAction SilentlyContinue
    if ($command) { return $command.Source }
    return ''
}
$CMake = if ($env:LAST_CONNECTION_CMAKE) { $env:LAST_CONNECTION_CMAKE } else { Find-CommandPath 'cmake' }
$Ninja = if ($env:LAST_CONNECTION_NINJA) { $env:LAST_CONNECTION_NINJA } else { Find-CommandPath 'ninja' }
$gccCommand = Find-CommandPath 'g++'
$GccRoot = if ($env:LAST_CONNECTION_GCC_ROOT) { $env:LAST_CONNECTION_GCC_ROOT } elseif ($gccCommand) { Split-Path -Parent $gccCommand } else { '' }
$EmsdkRoot = if ($env:LAST_CONNECTION_EMSDK) { $env:LAST_CONNECTION_EMSDK } else { $env:EMSDK }
$localTools = Join-Path $ProjectRoot '.local/toolchain.local.ps1'
if (Test-Path -LiteralPath $localTools) { . $localTools }

function Assert-Tool([string]$Path) {
    if (-not $Path -or -not (Test-Path -LiteralPath $Path)) { throw "Required tool missing: $Path. See docs/BUILD.md and scripts/check_environment.ps1." }
}
function Invoke-Checked([string]$Executable, [string[]]$Arguments) {
    & $Executable @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Executable failed with exit code $LASTEXITCODE" }
}
function Set-BuildEnvironment {
    Assert-Tool $CMake
    Assert-Tool $Ninja
    $env:PATH = @((Split-Path -Parent $CMake), (Split-Path -Parent $Ninja), $GccRoot, $env:PATH) -join [IO.Path]::PathSeparator
    $taskTemp = Join-Path $ProjectRoot '.cache\tmp'
    New-Item -ItemType Directory -Force -Path $taskTemp | Out-Null
    $env:TEMP = $taskTemp
    $env:TMP = $taskTemp
}
function Get-PreviewPython {
    if ($env:LAST_CONNECTION_PYTHON -and (Test-Path -LiteralPath $env:LAST_CONNECTION_PYTHON)) {
        return $env:LAST_CONNECTION_PYTHON
    }
    $sdkPython = Get-ChildItem "$EmsdkRoot\python\*\python.exe" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($sdkPython) { return $sdkPython.FullName }
    $pythonCommand = Get-Command python -ErrorAction SilentlyContinue
    if ($pythonCommand) { return $pythonCommand.Source }
    throw 'Set LAST_CONNECTION_PYTHON to a working Python 3 interpreter.'
}
