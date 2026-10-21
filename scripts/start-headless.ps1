#requires -Version 5.1
# Stable owner-only Task Scheduler entry. No arguments, credentials or commands.
[CmdletBinding()]param()
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
# Public pre-execution boundary; launch callback permits security/persistence tests
# without starting any engine/admin job or using real credentials.
function Invoke-WorkerVerifiedImage {
 param([string]$Root,[string[]]$ApprovedHashes,[string]$OwnerSid,[scriptblock]$Launch)
 $c=Get-Content -LiteralPath (Join-Path $Root 'current.json') -Raw|ConvertFrom-Json
 $directory=[IO.Path]::GetFullPath([string]$c.directory);$prefix=[IO.Path]::GetFullPath((Join-Path $Root 'versions'))+[IO.Path]::DirectorySeparatorChar
 if(-not $directory.StartsWith($prefix,[StringComparison]::OrdinalIgnoreCase)){throw 'Version escaped root'}
 $exe=Join-Path $directory 'OrganizerWorker.exe'
 foreach($path in @($directory,$exe)){if((Get-Item -LiteralPath $path).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse leaf'}}
 if((Get-FileHash -LiteralPath $exe).Hash -ine $c.sha256){throw 'Image hash differs'}
 & $Launch $exe
}
function Invoke-WorkerHeadlessLifetime {
 param([scriptblock]$Launch)
 return (& $Launch)
}
if($MyInvocation.InvocationName -eq '.'){return}
if($args.Count){throw 'Headless launcher accepts no arguments'}
$i=[Security.Principal.WindowsIdentity]::GetCurrent();$p=New-Object Security.Principal.WindowsPrincipal($i)
if($p.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator) -or $i.User.Value -in @('S-1-5-18','S-1-5-19','S-1-5-20')){throw 'Headless engine requires the normal LIMITED owner'}
$root=Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'OrganizerWorker'
$enrollment=Join-Path $root 'unattended/enrollment.json'
foreach($f in @($root,$enrollment,(Join-Path $root 'current.json'))){$item=Get-Item -LiteralPath $f;if($item.Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse launch state refused'}}
$e=Get-Content -LiteralPath $enrollment -Raw|ConvertFrom-Json
if($e.schema -ne 1 -or $e.state -cne 'enrolled' -or $e.owner_sid -cne $i.User.Value -or $e.task_name -cne ('OrganizerWorker-Unattended-'+$i.User.Value)){throw 'Existing enrolled owner/profile required'}
$c=Get-Content -LiteralPath (Join-Path $root 'current.json') -Raw|ConvertFrom-Json
$directory=[IO.Path]::GetFullPath([string]$c.directory);$allowed=[IO.Path]::GetFullPath((Join-Path $root 'versions'))+[IO.Path]::DirectorySeparatorChar
if(-not $directory.StartsWith($allowed,[StringComparison]::OrdinalIgnoreCase)){throw 'Installed version escapes owner root'}
if((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse installed version refused'}
$exe=Join-Path $directory 'OrganizerWorker.exe'
if((Get-Item -LiteralPath $exe).Attributes -band [IO.FileAttributes]::ReparsePoint){throw 'Reparse engine refused'}
if($c.sha256 -notmatch '\A[a-f0-9]{64}\z' -or (Get-FileHash -LiteralPath $exe).Hash -ine $c.sha256){throw 'Installed executable hash mismatch'}
# Wait the actual no-UI process for its entire lifetime: task retry policy sees
# real failures, not a successful detached submission. No profile/DPAPI bypass.
$start=New-Object Diagnostics.ProcessStartInfo
$start.FileName=$exe;$start.Arguments='--headless';$start.WorkingDirectory=$root;$start.UseShellExecute=$false;$start.CreateNoWindow=$true
$child=New-Object Diagnostics.Process;$child.StartInfo=$start
if(-not $child.Start()){throw 'Headless process did not start'}
try{$child.WaitForExit();$code=$child.ExitCode}finally{$child.Dispose()}
exit $code
