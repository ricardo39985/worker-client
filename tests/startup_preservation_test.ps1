$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../scripts/setup-startup.ps1')
# Only public Windows boundary adapters. No real task/Startup folder mutation.
$script:registrations=0
function Get-ScheduledTask {param($TaskName,$ErrorAction)return [pscustomobject]@{Principal=[pscustomobject]@{RunLevel='Highest';UserId='foreign';LogonType='ServiceAccount'}}}
function New-ScheduledTaskAction {param($Execute,$Argument)return 'unused'}
function New-ScheduledTaskTrigger {param([switch]$AtLogOn,$User)return 'unused'}
function New-ScheduledTaskPrincipal {param($UserId,$LogonType,$RunLevel)return 'unused'}
function New-ScheduledTaskSettingsSet {param($ExecutionTimeLimit,$MultipleInstances,[switch]$AllowStartIfOnBatteries,[switch]$DontStopIfGoingOnBatteries)return 'unused'}
function Register-ScheduledTask {param($TaskName,$Action,$Trigger,$Principal,$Settings,$Description,[switch]$Force)$script:registrations++;throw 'Unsafe attempted overwrite intercepted'}
function New-Item {param($ItemType,$Path,[switch]$Force)throw 'Real Startup writes forbidden in this contract'}
function New-Object {param($ComObject)throw 'Real shortcut COM writes forbidden in this contract'}
$rejected=$false;try{Register-WorkerStartup -Root ([IO.Path]::Combine([IO.Path]::GetTempPath(),'ow-no-owned-task')) -Sid 'S-1-5-21-123'}catch{$rejected=$true}
if(-not $rejected -or $script:registrations -ne 0){throw 'Unknown pre-existing startup task was overwritten or silently repaired; preserve all existing registration'}
Write-Output 'STARTUP_PRESERVATION_PASS unknown prior task retained, no fallback registration/deletion'
