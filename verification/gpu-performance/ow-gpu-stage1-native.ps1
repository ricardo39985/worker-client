Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$source=Join-Path $env:USERPROFILE 'ow-gpu-stage1-20261017'
$archive=Join-Path $env:USERPROFILE 'ow-gpu-stage1-source.zip'
if(Test-Path -LiteralPath $source){throw 'Isolated source already exists; refusing reuse'}
Expand-Archive -LiteralPath $archive -DestinationPath $source
& (Join-Path $source 'verification\gpu-performance\run-native-gate.ps1')
exit $LASTEXITCODE
