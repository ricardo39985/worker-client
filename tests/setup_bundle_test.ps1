#requires -Version 5.1
$ErrorActionPreference='Stop';Set-StrictMode -Version Latest
. (Join-Path $PSScriptRoot '../scripts/setup-policy.ps1')
. (Join-Path $PSScriptRoot '../scripts/setup-bundle.ps1')
. (Join-Path $PSScriptRoot '../scripts/setup-inference.ps1')
function Assert($value) {if(-not $value){throw 'Assertion failed'}}
function Rejects([scriptblock]$body){$failed=$false;try{& $body | Out-Null}catch{$failed=$true};Assert $failed}
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-bundle-contract-'+[guid]::NewGuid().ToString('N'));New-Item -ItemType Directory $root | Out-Null
try {
 $bundle=Join-Path $root 'bundle';New-Item -ItemType Directory $bundle|Out-Null;$files=@()
 foreach($name in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe'){
  $file=Join-Path $bundle $name;[IO.File]::WriteAllText($file,'synthetic native package bytes');$files+=@{path=$name;sha256=(Get-WorkerFileSha256 $file -Quiet)}
 }
 Write-WorkerJson (Join-Path $bundle 'manifest.json') @{schema=1;architecture='x64';version='0.1.3';revision=('a'*40);native_tests='passed';files=$files}
 # No source tree, compiler, Boost, network or system dependency is present.
 $installed=Install-WorkerBundle $bundle (Join-Path $root 'installation');Assert (Test-Path (Join-Path $installed 'OrganizerWorker.exe'))
 [IO.File]::WriteAllText((Join-Path $bundle 'OrganizerWorker.exe'),'damaged');Rejects {Install-WorkerBundle $bundle (Join-Path $root 'rejected')}
 Assert (-not (Test-Path (Join-Path $root 'rejected')))
 # Changing only the console produces a distinct version; rollback cannot
 # accidentally mutate the previous executable directory.
 [IO.File]::WriteAllText((Join-Path $bundle 'OrganizerWorker.exe'),'synthetic native package bytes')
 [IO.File]::WriteAllText((Join-Path $bundle 'OrganizerWorkerConsole.exe'),'new console bytes')
 $files[1].sha256=Get-WorkerFileSha256 (Join-Path $bundle 'OrganizerWorkerConsole.exe') -Quiet
 Write-WorkerJson (Join-Path $bundle 'manifest.json') @{schema=1;architecture='x64';version='0.1.3';revision=('a'*40);native_tests='passed';files=$files}
 $updated=Install-WorkerBundle $bundle (Join-Path $root 'installation');Assert ($updated -ne $installed)
 Assert ([IO.File]::ReadAllText((Join-Path $installed 'OrganizerWorkerConsole.exe')) -eq 'synthetic native package bytes')
 $state=Join-Path $root 'state';New-Item -ItemType Directory $state|Out-Null
 foreach($name in 'current.json','worker.json','credential.dpapi','journal.sqlite3'){[IO.File]::WriteAllText((Join-Path $state $name),'retained-'+$name)}
 $stopped=@{value=$false}
 Rejects {Invoke-WorkerInstallTransaction -Root $state -Publish {[IO.File]::WriteAllText((Join-Path $state 'current.json'),'candidate');[IO.File]::WriteAllText((Join-Path $state 'worker.json'),'candidate')} -Verify {throw 'Launch failed'} -StopCandidate {$stopped.value=$true}}
 Assert $stopped.value
 foreach($name in 'current.json','worker.json','credential.dpapi','journal.sqlite3'){Assert ([IO.File]::ReadAllText((Join-Path $state $name)) -eq ('retained-'+$name))}
 # Recovery backup survives when a candidate cannot be stopped safely.
 Rejects {Invoke-WorkerInstallTransaction -Root $state -Publish {[IO.File]::WriteAllText((Join-Path $state 'current.json'),'unsafe-candidate')} -Verify {throw 'Launch failed'} -StopCandidate {throw 'Stop failed'}}
 Assert (@(Get-ChildItem -LiteralPath $state -Directory -Filter 'update-backup-*').Count -eq 1)
 # Explicit OS ports prove a clean-PC repair without installing on this host.
 Invoke-WorkerRuntimeBootstrap -Present {$true} -Approve {throw 'No license prompt for an existing runtime'} -Prepare {throw 'No download'} -Install {throw 'No installation'}
 $native=@{present=$false;installs=0}
 Rejects {Invoke-WorkerRuntimeBootstrap -Present {$native.present} -Approve {$false} -Prepare {throw 'Download without approval'} -Install {$native.installs++}}
 Assert ($native.installs -eq 0)
 Rejects {Invoke-WorkerRuntimeBootstrap -Present {$native.present} -Approve {$true} -Prepare {throw 'Signature invalid'} -Install {$native.installs++}}
 Assert ($native.installs -eq 0)
 Rejects {Invoke-WorkerRuntimeBootstrap -Present {$native.present} -Approve {$true} -Prepare {} -Install {$native.installs++}}
 Assert ($native.installs -eq 1) # A successful exit alone is insufficient.
 Invoke-WorkerRuntimeBootstrap -Present {$native.present} -Approve {$true} -Prepare {} -Install {$native.installs++;$native.present=$true}
 Assert $native.present
 Write-Host 'Bundle corruption, compiler-free install, rollback and retained-state contracts passed.'
} finally {Remove-Item -LiteralPath $root -Recurse -Force}
