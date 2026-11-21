param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$Test,
    [switch]$Install,
    [int]$Jobs = [Environment]::ProcessorCount
)
$arguments = @('--configuration', $Configuration, '--jobs', "$Jobs")
if ($Test) { $arguments += '--test' }
if ($Install) { $arguments += '--install' }
& "$PSScriptRoot/build.bat" @arguments
exit $LASTEXITCODE
