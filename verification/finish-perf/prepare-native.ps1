$ErrorActionPreference='Stop'
$stage=Join-Path $env:LOCALAPPDATA 'OrganizerWorker\perf-land-0771b79'
if(Test-Path $stage){throw 'Refusing to overwrite existing staging'}
New-Item -ItemType Directory -Path $stage | Out-Null
Write-Output ('INSTALL_STAGING='+$stage)
