. "$PSScriptRoot\toolchain.ps1"
$tools = [ordered]@{
    Git = (Get-Command git -ErrorAction SilentlyContinue).Source
    PowerShell = (Get-Process -Id $PID).Path
    CMake = $CMake
    Ninja = $Ninja
    GCC = "$GccRoot\g++.exe"
    Emscripten = "$EmsdkRoot\upstream\emscripten\emcc.bat"
    Python = (Get-PreviewPython)
}
foreach ($entry in $tools.GetEnumerator()) {
    $exists = $entry.Value -and (Test-Path -LiteralPath $entry.Value)
    [pscustomobject]@{ Tool = $entry.Key; Available = [bool]$exists; Path = $entry.Value }
}
