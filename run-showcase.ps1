param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [Parameter(ValueFromRemainingArguments=$true)][string[]]$ShowcaseArguments
)
$ErrorActionPreference = 'Stop'
$exe = Join-Path $PSScriptRoot "build/windows-vs2022/bin/$Configuration/jsd-showcase.exe"
if (!(Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Build first: install.bat' }
# Assets are found relative to the executable or in the source tree, independent of cwd.
& $exe @ShowcaseArguments
exit $LASTEXITCODE
