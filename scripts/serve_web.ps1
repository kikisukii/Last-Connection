param([ValidateRange(1024,65535)][int]$Port = 8000)
. "$PSScriptRoot\toolchain.ps1"
$webDir = Join-Path $ProjectRoot 'build\web'
Assert-Tool (Join-Path $webDir 'index.html')
$python = Get-PreviewPython
Write-Host "Preview: http://localhost:$Port - Ctrl+C stops the server."
Invoke-Checked $python @('-m', 'http.server', "$Port", '--bind', '127.0.0.1', '--directory', $webDir)
