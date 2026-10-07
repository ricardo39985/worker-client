#requires -Version 5.1
[CmdletBinding()]
param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$currentPath=Join-Path $root 'current.json'
. (Join-Path $root 'setup-policy.ps1')
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
if ((Get-WorkerFileSha256 $exe) -ine [string]$current.sha256) { throw 'Worker binary differs from installation manifest. Refusing launch.' }
if (Test-WorkerRuntime -Root $root -ExpectedExecutable $exe) {
    Write-Host 'Worker is already running with the installed version.'
    return
}
if (-not [Environment]::UserInteractive -or (Get-Process -Id $PID).SessionId -eq 0) {
    $taskName='OrganizerWorker-'+$identity.User.Value
    if (-not (Get-Command Get-ScheduledTask -ErrorAction SilentlyContinue)) { throw 'No interactive desktop. Launch locally after signing in to Windows.' }
    $task=Get-ScheduledTask -TaskName $taskName -ErrorAction SilentlyContinue
    if (-not $task) { throw 'No interactive startup task exists. Run setup.cmd once from your signed-in Windows desktop.' }
    Start-ScheduledTask -TaskName $taskName
} else {
    Start-Process -FilePath $exe -WorkingDirectory $root | Out-Null
}
$deadline=[datetime]::UtcNow.AddSeconds(20)
$progress=New-WorkerProgress 'Wait for worker process readiness'
try {
do {
    if (Test-WorkerRuntime -Root $root -ExpectedExecutable $exe) {
        Complete-WorkerProgress $progress -Detail 'Matching process image and readiness marker verified'
        Write-Host 'Worker started and verified. Closing the console leaves the system-tray worker running.'
        return
    }
    Update-WorkerProgress $progress -Detail 'Waiting for matching process and runtime marker'
    Start-Sleep -Milliseconds 200
} while ([datetime]::UtcNow -lt $deadline)
throw 'Worker did not report a matching running process. Check its startup dialog/log; Windows must have an interactive signed-in desktop.'
} catch {Complete-WorkerProgress $progress -Failed -Detail $_.Exception.Message;throw}
