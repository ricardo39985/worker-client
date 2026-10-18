#requires -Version 5.1
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '..\scripts\worker-swap.ps1')
$root=Join-Path $env:TEMP ('ow-swap-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
$passed=0;$failed=0
function Assert($Condition,[string]$Message='Assertion failed') {if(-not $Condition){throw $Message}}
function Case([string]$Name,[scriptblock]$Body) {
 try {& $Body;Write-Output "PASS $Name";$script:passed++}
 catch {Write-Output "FAIL $Name : $($_.Exception.Message)";$script:failed++}
}
function Fixture {
 $r=Join-Path $root ([guid]::NewGuid().ToString('N'))
 foreach($v in 'old','new') {
  $d=Join-Path $r ('versions\'+$v);New-Item -ItemType Directory -Path $d -Force | Out-Null
  Copy-Item $env:COMSPEC (Join-Path $d 'OrganizerWorker.exe')
 }
 $old=@{schema=1;directory=(Join-Path $r 'versions\old');sha256=(Get-FileHash $env:COMSPEC).Hash}
 $new=@{schema=1;directory=(Join-Path $r 'versions\new');sha256=$old.sha256}
 [IO.File]::WriteAllText((Join-Path $r 'current.json'),($old|ConvertTo-Json))
 return @{root=$r;old=$old;new=$new;bytes=[IO.File]::ReadAllText((Join-Path $r 'current.json'))}
}
function Runnable($f,[string]$Version='old') {
 $c=Get-Content (Join-Path $f.root 'current.json') -Raw | ConvertFrom-Json
 Assert ($c.directory -eq (Join-Path $f.root ('versions\'+$Version))) 'Wrong active version'
 $exe=Join-Path $c.directory 'OrganizerWorker.exe'
 Assert ((Get-FileHash $exe).Hash -eq $c.sha256) 'Manifest/binary mismatch'
 & $exe /d /c 'exit 0'
 Assert ($LASTEXITCODE -eq 0) 'Selected old binary is not runnable'
}
function LockedManifest($f) {
 $script="`$h=[IO.File]::Open('$(Join-Path $f.root 'current.json')',[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read);[IO.File]::WriteAllText('$(Join-Path $f.root 'locked')','ready');Start-Sleep -Seconds 40;`$h.Dispose()"
 $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($script))
 $p=Start-Process powershell.exe -ArgumentList @('-NoProfile','-NonInteractive','-EncodedCommand',$encoded) -PassThru -WindowStyle Hidden
 $null=$p.Handle
 $until=[datetime]::UtcNow.AddSeconds(10)
 while(-not (Test-Path (Join-Path $f.root 'locked')) -and [datetime]::UtcNow -lt $until){Start-Sleep -Milliseconds 50}
 Assert (Test-Path (Join-Path $f.root 'locked')) 'Lock holder did not become ready'
 return $p
}
try {
 Case 'Windows PowerShell null backup reproduces the rollout error; explicit backup publishes' {
  $f=Fixture;$temp=Join-Path $f.root 'new.json';[IO.File]::WriteAllText($temp,($f.new|ConvertTo-Json))
  $caught=$false
  try {[IO.File]::Replace($temp,(Join-Path $f.root 'current.json'),$null)}
  catch {$caught=$true;Write-Output ('REPRO_NULL_BACKUP='+$_.Exception.InnerException.GetType().Name+' '+$_.Exception.InnerException.Message)}
  Assert $caught 'Original call must fail on Windows PowerShell 5.1'
  Runnable $f
  Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {} -StartWorker {} -VerifyWorker {Runnable $f 'new'}
  Runnable $f 'new'
 }
 Case 'real sharing violation stops the supplied holder then retries the atomic swap' {
  $f=Fixture;$holder=LockedManifest $f;$state=@{released=$false}
  try {
   Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {} -StartWorker {} -VerifyWorker {Runnable $f 'new'} -OnSharingViolation {
    $state.released=$true;Stop-Process -Id $holder.Id;$holder.WaitForExit(5000)|Out-Null
   }
   Assert $state.released 'Sharing violation was not exercised';Runnable $f 'new'
  } finally {if(-not $holder.HasExited){$holder.Kill();$holder.WaitForExit()};$holder.Dispose()}
 }
 Case 'persistent lock fails within its bound and keeps the exact old manifest runnable' {
  $f=Fixture;$holder=LockedManifest $f;$caught=$null;$watch=[Diagnostics.Stopwatch]::StartNew()
  try {Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {} -StartWorker {} -VerifyWorker {} -RetryMilliseconds 300}
  catch {$caught=$_}
  finally {$holder.Kill();$holder.WaitForExit();$holder.Dispose()}
  Assert ($null -ne $caught) 'Persistent sharing violation must fail'
  Assert ($watch.Elapsed.TotalSeconds -lt 5) 'Retry was not bounded'
  Assert ($caught.Exception.Message -match 'File.Replace' -and $caught.Exception.Message.Contains((Join-Path $f.root 'current.json'))) 'Failure omits exact call and target'
  Assert ([IO.File]::ReadAllText((Join-Path $f.root 'current.json')) -ceq $f.bytes) 'Old manifest changed'
  Runnable $f
 }
 Case 'failed candidate launch restores the old manifest and launches the old version' {
  $f=Fixture;$state=@{starts=0};$caught=$false
  try {Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {} -StartWorker {$state.starts++} -VerifyWorker {
   $c=Get-Content (Join-Path $f.root 'current.json') -Raw|ConvertFrom-Json
   if($c.directory -eq $f.new.directory){throw 'synthetic readiness failure'}
   Runnable $f
  }} catch {$caught=$true}
  Assert $caught;Assert ($state.starts -eq 2) 'Old version was not restarted'
  Assert ([IO.File]::ReadAllText((Join-Path $f.root 'current.json')) -ceq $f.bytes);Runnable $f
 }
 Case 'abrupt installer termination after publish retains recovery and restores a runnable old version' {
  $f=Fixture;$module=(Resolve-Path (Join-Path $PSScriptRoot '..\scripts\worker-swap.ps1')).Path
  $s=". '$module';`$m=Get-Content '$(Join-Path $f.root 'candidate.json')' -Raw|ConvertFrom-Json;Invoke-WorkerVersionSwap -Root '$($f.root)' -Manifest `$m -StopWorker {} -StartWorker {} -VerifyWorker {[IO.File]::WriteAllText('$(Join-Path $f.root 'published')','yes');Start-Sleep -Seconds 60}"
  [IO.File]::WriteAllText((Join-Path $f.root 'candidate.json'),($f.new|ConvertTo-Json))
  $p=Start-Process powershell.exe -ArgumentList @('-NoProfile','-NonInteractive','-ExecutionPolicy','Bypass','-EncodedCommand',([Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($s)))) -WindowStyle Hidden -PassThru
  $null=$p.Handle
  try {
   $until=[datetime]::UtcNow.AddSeconds(10)
   while(-not (Test-Path (Join-Path $f.root 'published')) -and [datetime]::UtcNow -lt $until){Start-Sleep -Milliseconds 50}
   Assert (Test-Path (Join-Path $f.root 'published')) 'Child never published'
   $p.Kill();$p.WaitForExit();Runnable $f 'new'
   Restore-WorkerVersion -Root $f.root -StopWorker {} -StartWorker {} -VerifyWorker {Runnable $f}
   Assert ([IO.File]::ReadAllText((Join-Path $f.root 'current.json')) -ceq $f.bytes);Runnable $f
  } finally {if(-not $p.HasExited){$p.Kill();$p.WaitForExit()};$p.Dispose()}
 }
 Case 'locked recovery file during commit keeps recovery and restores the old runnable version' {
  $f=Fixture;$state=@{handle=$null};$caught=$false
  try {
   try {Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {} -StartWorker {} -VerifyWorker {
    if($null -eq $state.handle){$state.handle=[IO.File]::Open((Join-Path $f.root 'current.rollback.json'),[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)}
   }} catch {$caught=$true}
   Assert $caught 'Locked commit recovery must fail explicitly'
   Assert (Test-Path (Join-Path $f.root 'current.rollback.json')) 'Recovery copy was lost'
   Runnable $f
  } finally {if($state.handle){$state.handle.Dispose()}}
  Restore-WorkerVersion -Root $f.root -StopWorker {} -StartWorker {} -VerifyWorker {Runnable $f}
 }
 Case 'incomplete candidate is rejected before stopping or changing the current version' {
  $f=Fixture;Remove-Item (Join-Path $f.new.directory 'OrganizerWorker.exe');$caught=$false
  try {Invoke-WorkerVersionSwap -Root $f.root -Manifest $f.new -StopWorker {throw 'must not stop'} -StartWorker {} -VerifyWorker {}}
  catch {$caught=$true;Assert ($_.Exception.Message -notmatch 'must not stop')}
  Assert $caught;Runnable $f
 }
} finally {Remove-Item $root -Recurse -Force}
Write-Output "$passed swap contracts passed; $failed failed"
if($failed){exit 1}
