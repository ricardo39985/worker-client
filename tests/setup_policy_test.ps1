#requires -Version 5.1
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '..\scripts\setup-policy.ps1')
. (Join-Path $PSScriptRoot '..\scripts\setup-dependencies.ps1')
$script:failed=0;$script:passed=0
function Assert($Condition,[string]$Message='Assertion failed') { if (-not $Condition) { throw $Message } }
function Rejects([scriptblock]$Body) { $caught=$false;try { & $Body | Out-Null } catch { $caught=$true };Assert $caught 'Expected explicit failure' }
function Case([string]$Name,[scriptblock]$Body) {try {& $Body;Write-Host "PASS $Name";$script:passed++} catch {Write-Host "FAIL $Name : $($_.Exception.Message)";$script:failed++}}
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-setup-test-'+[guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $root | Out-Null
try {
 Case 'expired pairing is identified without deleting identity and healthy credentials are retained' {
  $action=Get-WorkerPairingAction -Origin 'https://worker.test' -NowMs 10000 -ReadSecret {
    param($slot)
    if($slot -eq 'pending'){[pscustomobject]@{coordinator_url='https://worker.test';expires_at_ms=9000}}else{$null}
  }
  Assert ($action.kind -eq 're-pair')
  $action=Get-WorkerPairingAction -Origin 'https://worker.test' -NowMs 10000 -ReadSecret {
    param($slot)
    if($slot -eq 'credential'){[pscustomobject]@{coordinator_url='https://worker.test'}}else{throw 'credential takes precedence'}
  }
  Assert ($action.kind -eq 'none')
 }
 Case 'unreadable or cross-endpoint identity requires explicit full archival' {
  $action=Get-WorkerPairingAction -Origin 'https://worker.test' -ReadSecret {throw 'cannot decrypt'}
  Assert ($action.kind -eq 'archive')
  $action=Get-WorkerPairingAction -Origin 'https://worker.test' -ReadSecret {param($slot)[pscustomobject]@{coordinator_url='https://other.test'}}
  Assert ($action.kind -eq 'archive')
 }
 Case 'verified dependency tree is reused but changed or missing files are repaired' {
  $directory=Join-Path $root 'dependency';$sha='a'*64;$state=@{extracts=0}
  $extract={param($stage)$state.extracts++;[IO.File]::WriteAllText((Join-Path $stage 'library.hpp'),'trusted source')}
  Expand-WorkerDependency $directory $sha $extract
  Assert (Test-WorkerDependencyManifest $directory $sha)
  Expand-WorkerDependency $directory $sha {throw 'healthy cache must be reused'}
  [IO.File]::WriteAllText((Join-Path $directory 'library.hpp'),'damaged source')
  Expand-WorkerDependency $directory $sha $extract
  Assert ($state.extracts -eq 2)
  Assert ([IO.File]::ReadAllText((Join-Path $directory 'library.hpp')) -eq 'trusted source')
 }
 Case 'failed fresh extraction preserves existing files and unrelated identity' {
  $directory=Join-Path $root 'failed-dependency';New-Item -ItemType Directory -Path $directory | Out-Null
  [IO.File]::WriteAllText((Join-Path $directory 'retained.bin'),'old bytes')
  $identity=Join-Path $root 'retained-credential';[IO.File]::WriteAllText($identity,'identity bytes')
  Rejects {Expand-WorkerDependency $directory ('b'*64) {param($stage)throw 'synthetic extraction failure'}}
  Assert ([IO.File]::ReadAllText((Join-Path $directory 'retained.bin')) -eq 'old bytes')
  Assert ([IO.File]::ReadAllText($identity) -eq 'identity bytes')
 }
 Case 'bare DNS input selects verified HTTPS and invalid interactive input can be corrected' {
  Assert ((ConvertTo-WorkerOrigin 'worker.example.test') -eq 'https://worker.example.test')
  $state=@{n=0}
  $value=Resolve-WorkerOrigin -Prompt {$state.n++;if($state.n -eq 1){'http://wrong.test'}else{'worker.example.test'}}
  Assert ($value -eq 'https://worker.example.test');Assert ($state.n -eq 2)
 }
 Case 'repair retries recoverable setup once and preserves user state' {
  $state=@{runs=0;repairs=0;credential='preserve';journal='preserve'}
  $value=Invoke-WorkerRepair -Operation {$state.runs++;if($state.runs -eq 1){throw 'damaged cache'};'healthy'} -Repair {$state.repairs++}
  Assert ($value -eq 'healthy');Assert ($state.repairs -eq 1)
  Assert ($state.credential -eq 'preserve' -and $state.journal -eq 'preserve')
 }
 Case 'persistent failure stops after bounded repair and healthy step never repairs' {
  $state=@{runs=0;repairs=0}
  Rejects {Invoke-WorkerRepair -Operation {$state.runs++;throw 'still broken'} -Repair {$state.repairs++}}
  Assert ($state.runs -eq 2 -and $state.repairs -eq 1)
  Assert ((Invoke-WorkerRepair -Operation {'healthy'} -Repair {throw 'must not repair'}) -eq 'healthy')
 }
 Case 'damaged checksum cache is replaced and unverified replacement is never retained' {
  $path=Join-Path $root 'download.bin';[IO.File]::WriteAllText($path,'damaged')
  $expected='2689367b205c16ce32ed4200942b8b8b1e262dfc70d9bc9fbc77c49699a4f1df'
  Get-WorkerVerifiedFile -Destination $path -Sha256 $expected -Download {param($temp)[IO.File]::WriteAllText($temp,'ok')}
  Assert ([IO.File]::ReadAllText($path) -eq 'ok')
  [IO.File]::WriteAllText($path,'damaged')
  Rejects {Get-WorkerVerifiedFile -Destination $path -Sha256 $expected -Download {param($temp)[IO.File]::WriteAllText($temp,'untrusted')}}
  Assert (-not (Test-Path -LiteralPath $path))
 }
 Case 'startup repairs a failed verification before launch and never elevates the worker' {
  $state=@{registered=0;verified=0;launched=$false}
  $value=Complete-WorkerSetup -RegisterStartup {$state.registered++} -VerifyStartup {$state.verified++;$state.verified -gt 1} -LaunchWorker {$state.launched=$true}
  Assert ($state.registered -eq 2 -and $value.started -and $state.launched)
 }
 Case 'Windows account aliases identify the same startup owner by SID' {
  $identity=[Security.Principal.WindowsIdentity]::GetCurrent()
  $expected=$identity.User.Value
  Assert (Test-WorkerAccountSid -Account $expected -ExpectedSid $expected)
  Assert (Test-WorkerAccountSid -Account $identity.Name -ExpectedSid $expected)
  Assert (Test-WorkerAccountSid -Account ($identity.Name.Split('\')[-1]) -ExpectedSid $expected)
 }
 Case 'another account SID cannot own this users startup task' {
  $expected=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
  $other=if($expected -eq 'S-1-5-18'){'S-1-5-19'}else{'S-1-5-18'}
  Assert (-not (Test-WorkerAccountSid -Account $other -ExpectedSid $expected))
 }
 Case 'missing or unresolvable startup owner fails closed' {
  $expected=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
  Assert (-not (Test-WorkerAccountSid -Account '' -ExpectedSid $expected))
  Assert (-not (Test-WorkerAccountSid -Account ('missing-'+[guid]::NewGuid().ToString('N')) -ExpectedSid $expected))
 }
 Case 'first setup asks for endpoint and canonicalizes it' {
  $state=@{ prompts=0 }
  $result=Resolve-WorkerOrigin -Prompt {$state.prompts++;' HTTPS://WORKER.Example.Test:443/ '}
  Assert ($result -eq 'https://worker.example.test');Assert ($state.prompts -eq 1)
 }
 Case 'existing endpoint is reused without a prompt' {
  $value=Resolve-WorkerOrigin -Stored 'https://worker.example.test' -HasIdentity $true -Prompt {throw 'Must not prompt'}
  Assert ($value -eq 'https://worker.example.test')
 }
 Case 'blank or unsafe endpoint fails instead of choosing production' {
  foreach ($bad in '','http://host.test','wss://host.test/worker/connect','https://x:y@host.test','https://host.test:8443','https://host.test/path','https://host.test/?x=1','https://host.test/#x','https://host.test.','https://a..test','https://host.test//',"https://host.test`r`nX:1") {
   Rejects { ConvertTo-WorkerOrigin $bad }
  }
  Rejects { Resolve-WorkerOrigin -NonInteractive -Prompt {throw 'Must not prompt'} }
 }
 Case 'changing endpoint with identity requires explicit re-pair' {
  Rejects {Resolve-WorkerOrigin -Stored 'https://old.test' -Requested 'https://new.test' -HasIdentity $true}
  Assert ((Resolve-WorkerOrigin -Stored 'https://old.test' -Requested 'https://new.test' -HasIdentity $true -RePair) -eq 'https://new.test')
 }
 Case 'ordinary setup registers verifies and starts without opt-in switches' {
  $state=@{ registered=$false; verified=$false; launched=$false }
  $result=Complete-WorkerSetup -RegisterStartup {$state.registered=$true} -VerifyStartup {Assert $state.registered;$state.verified=$true;$true} -LaunchWorker {Assert $state.verified;$state.launched=$true}
  Assert ($state.registered -and $state.verified -and $state.launched)
  Assert ($result.started -and $result.startup_registered)
 }
 Case 'failed startup verification cannot report success or launch' {
  $state=@{ launched=$false }
  Rejects {Complete-WorkerSetup -RegisterStartup {} -VerifyStartup {$false} -LaunchWorker {$state.launched=$true}}
  Assert (-not $state.launched)
 }
 Case 'explicit maintenance opt-outs suppress only requested side effects' {
  $result=Complete-WorkerSetup -NoStartup -NoLaunch -RegisterStartup {throw 'must not register'} -VerifyStartup {throw 'must not verify'} -LaunchWorker {throw 'must not launch'}
  Assert (-not $result.started -and -not $result.startup_registered)
 }
 Case 'atomic JSON replacement preserves old bytes when publication fails' {
  $path=Join-Path $root 'config.json'
  Write-WorkerJson -Path $path -Value @{schema=1;endpoint='https://old.test'}
  $before=[IO.File]::ReadAllText($path)
  Rejects {Write-WorkerJson -Path $path -Value @{schema=1;endpoint='https://new.test'} -ReplaceFile {throw 'synthetic disk failure'}}
  Assert ([IO.File]::ReadAllText($path) -eq $before)
  Write-WorkerJson -Path $path -Value @{schema=1;endpoint='https://new.test'}
  Assert ((Get-Content -Raw $path | ConvertFrom-Json).endpoint -eq 'https://new.test')
 }
 Case 'readiness needs the matching process image and creation time not a stale PID' {
  $exe=Join-Path $root 'worker.exe';$start=[datetime]::UtcNow
  Write-WorkerJson (Join-Path $root 'runtime.json') @{schema=1;state='running';process_id=1234;process_started_filetime=$start.ToFileTimeUtc()}
  Assert (Test-WorkerRuntime -Root $root -ExpectedExecutable $exe -GetProcess {param($ProcessId) Assert ($ProcessId -eq 1234);[pscustomobject]@{Path=$exe;StartTime=$start;HasExited=$false}})
  Assert (-not (Test-WorkerRuntime -Root $root -ExpectedExecutable $exe -GetProcess {param($ProcessId) [pscustomobject]@{Path=$exe;StartTime=$start.AddSeconds(1);HasExited=$false}}))
  Assert (-not (Test-WorkerRuntime -Root $root -ExpectedExecutable $exe -GetProcess {param($ProcessId) [pscustomobject]@{Path='wrong.exe';StartTime=$start;HasExited=$false}}))
 }
 Case 'changing coordinators archives the old journal and keys instead of sending old results' {
  $scope=Join-Path $root 'scope';New-Item -ItemType Directory -Path $scope | Out-Null
  foreach ($name in 'credential.dpapi','pending-pairing.dpapi','pairing-key.dpapi','journal.sqlite3','journal.sqlite3-wal','worker.json') {[IO.File]::WriteAllText((Join-Path $scope $name),'old-'+$name)}
  New-Item -ItemType Directory -Path (Join-Path $scope 'jobs') | Out-Null
  [IO.File]::WriteAllText((Join-Path $scope 'jobs\retained.bin'),'old original bytes')
  $archive=Archive-WorkerPairing -Root $scope -WholeCoordinator
  Assert (-not (Test-Path (Join-Path $scope 'journal.sqlite3')))
  Assert (-not (Test-Path (Join-Path $scope 'pairing-key.dpapi')))
  Assert ([IO.File]::ReadAllText((Join-Path $archive 'journal.sqlite3')) -eq 'old-journal.sqlite3')
  Assert ([IO.File]::ReadAllText((Join-Path $archive 'jobs\retained.bin')) -eq 'old original bytes')
  Assert (Test-Path (Join-Path $scope 'worker.json'))
 }
 Case 'explicit re-pair archives encrypted identity but retains key and job journal'  {
  foreach ($name in 'credential.dpapi','pending-pairing.dpapi','pairing-key.dpapi','journal.sqlite3') {[IO.File]::WriteAllText((Join-Path $root $name),'synthetic-'+$name)}
  $archive=Archive-WorkerPairing $root
  Assert (-not (Test-Path (Join-Path $root 'credential.dpapi')))
  Assert ([IO.File]::ReadAllText((Join-Path $archive 'credential.dpapi')) -eq 'synthetic-credential.dpapi')
  Assert ([IO.File]::ReadAllText((Join-Path $root 'pairing-key.dpapi')) -eq 'synthetic-pairing-key.dpapi')
  Assert ([IO.File]::ReadAllText((Join-Path $root 'journal.sqlite3')) -eq 'synthetic-journal.sqlite3')
 }
} finally {Remove-Item -LiteralPath $root -Recurse -Force}
Write-Host "$script:passed setup contracts passed; $script:failed failed"
if($script:failed){exit 1}
