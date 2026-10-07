#requires -Version 5.1
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$currentPath=Join-Path $root 'current.json'
if (-not (Test-Path -LiteralPath $currentPath)) { throw 'Worker has not been installed. Run setup.cmd first.' }
$identity=[Security.Principal.WindowsIdentity]::GetCurrent()
$principal=New-Object Security.Principal.WindowsPrincipal($identity)
if ($principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) { throw 'Start the worker as your normal Windows user, not Administrator.' }
$current=Get-Content -LiteralPath $currentPath -Raw | ConvertFrom-Json
$directory=[IO.Path]::GetFullPath([string]$current.directory)
$allowed=[IO.Path]::GetFullPath((Join-Path $root 'versions'))+[IO.Path]::DirectorySeparatorChar
if (-not $directory.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid installed version path.' }
$exe=Join-Path $directory 'OrganizerWorker.exe'
if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) { throw 'Worker binary is missing. Rerun setup.' }
if ((Get-FileHash -LiteralPath $exe -Algorithm SHA256).Hash -ine [string]$current.sha256) { throw 'Worker binary differs from installation manifest. Refusing launch.' }
if (-not [Environment]::UserInteractive -or (Get-Process -Id $PID).SessionId -eq 0) {
    $taskName='OrganizerWorker-'+$identity.User.Value
    if (-not (Get-Command Get-ScheduledTask -ErrorAction SilentlyContinue)) { throw 'No interactive desktop. Launch locally after signing in to Windows.' }
    $task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if (-not $task) { throw 'SSH can update the worker but cannot display a tray icon in a service session. Launch once locally, or register the interactive logon task with setup.cmd -StartAtLogon.' }
    Start-ScheduledTask -TaskName $taskName
    Write-Host 'Interactive logon task requested. The worker requires its Windows user to be signed in.'
} else {
    Start-Process -FilePath $exe -WorkingDirectory $root
    Write-Host 'Worker launched. Close its console to leave it running in the system tray.'
}
