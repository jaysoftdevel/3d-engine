param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$Test,
    [switch]$Install,
    [int]$Jobs = [Environment]::ProcessorCount
)
$ErrorActionPreference = 'Stop'
$env:PYTHONDONTWRITEBYTECODE = '1'
$python = (Get-Command python -ErrorAction Stop).Source
$arguments = @('-B', (Join-Path $PSScriptRoot 'tools/build_windows.py'), '--root', $PSScriptRoot,
               '--configuration', $Configuration, '--jobs', "$Jobs")
if ($Test) { $arguments += '--test' }
if ($Install) { $arguments += '--install' }
& $python @arguments
if ($LASTEXITCODE -ne 0) { throw "Engine build failed: $LASTEXITCODE" }
