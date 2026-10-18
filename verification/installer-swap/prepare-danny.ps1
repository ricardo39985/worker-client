$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-installer-swap-20261018'
if(Test-Path $stage){throw 'Refusing an existing staging directory'}
New-Item -ItemType Directory -Path $stage,(Join-Path $stage 'tmp') | Out-Null
Write-Output ('STAGING_CREATED='+$stage)
