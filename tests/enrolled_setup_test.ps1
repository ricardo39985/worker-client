param([Parameter(Mandatory=$true)][string]$SourceRoot)
Set-StrictMode -Version Latest;$ErrorActionPreference='Stop'
. (Join-Path $SourceRoot 'scripts/setup-policy.ps1')
$base=Join-Path $env:TEMP ('ow-enrolled-setup-'+[guid]::NewGuid().ToString('N'));$root=Join-Path $base 'owner';$old=Join-Path $root 'versions/old';$candidate=Join-Path $root 'versions/new'
[IO.Directory]::CreateDirectory($old)|Out-Null;[IO.Directory]::CreateDirectory($candidate)|Out-Null
$names=@('OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe','OrganizerWorkerTray.exe','cli.md','start-headless.ps1')
foreach($name in $names){[IO.File]::WriteAllText((Join-Path $old $name),'synthetic-'+$name);Copy-Item (Join-Path $old $name) (Join-Path $candidate $name)}
@{schema=1;directory=$old;sha256=(Get-FileHash (Join-Path $old 'OrganizerWorker.exe')).Hash.ToLowerInvariant()}|ConvertTo-Json|Set-Content (Join-Path $root 'current.json')
[IO.File]::WriteAllText((Join-Path $root 'credential.dpapi'),'SYNTHETIC-SEALED-NOT-A-CREDENTIAL')
try{
 Assert-WorkerEnrolledSetupVersion $root $candidate
 [IO.Directory]::CreateDirectory((Join-Path $root 'unattended'))|Out-Null
 foreach($state in @('enrolled','pairing_repair')){
  @{schema=1;state=$state}|ConvertTo-Json|Set-Content (Join-Path $root 'unattended/enrollment.json')
  Assert-WorkerEnrolledSetupVersion $root $candidate
  foreach($name in @('OrganizerWorker.exe','OrganizerWorkerCtl.exe','start-headless.ps1')){
   [IO.File]::AppendAllText((Join-Path $candidate $name),'foreign-change')
   $rejected=$false;try{Assert-WorkerEnrolledSetupVersion $root $candidate}catch{$rejected=$true}
   if(-not $rejected){throw ($state+': full setup would archive pairing/publish unapproved '+$name+' before canonical approval')}
   Copy-Item (Join-Path $old $name) (Join-Path $candidate $name) -Force
  }
 }
 if([IO.File]::ReadAllText((Join-Path $root 'credential.dpapi')) -cne 'SYNTHETIC-SEALED-NOT-A-CREDENTIAL'){throw 'Pairing touched by version preflight'}
 'ENROLLED_SETUP_PASS same-code maintenance retained; unapproved code rejected before pairing/config/current publication'
}finally{Remove-Item -LiteralPath $base -Recurse -Force}
