Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
$stage=Join-Path $env:LOCALAPPDATA 'Temp\ow-worker-flex-20261018-r2'
$env:TEMP=Join-Path $stage 'tmp';$env:TMP=$env:TEMP;$env:PSModuleAnalysisCachePath=Join-Path $stage 'module-cache'
$test=Join-Path $stage ('installer-contract-'+[guid]::NewGuid().ToString('N'))
$kit=Join-Path $test 'kit';New-Item -ItemType Directory $kit -Force|Out-Null
Copy-Item (Join-Path $stage 'install-qualified.ps1') (Join-Path $kit 'install-qualified.ps1')
Copy-Item (Join-Path $stage 'candidate\source\scripts\worker-swap.ps1') (Join-Path $kit 'worker-swap.ps1')
@'
function Test-WorkerAccountSid {param($Account,$ExpectedSid) return $Account -eq $ExpectedSid}
function Test-WorkerRuntime {param($Root,$ExpectedExecutable) return (Test-Path -LiteralPath $ExpectedExecutable)}
'@ | Set-Content -LiteralPath (Join-Path $kit 'setup-policy.ps1') -Encoding UTF8
$global:fakeRoot='';$global:failCandidate=$false;$global:oldDirectory=''
function Get-ScheduledTask { [CmdletBinding()]param($TaskName)
 return [pscustomobject]@{Principal=[pscustomobject]@{RunLevel='Limited';LogonType='Interactive';UserId=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value}}
}
function Get-CimInstance { [CmdletBinding()]param($ClassName)
 return [pscustomobject]@{FreePhysicalMemory=8*1024*1024}
}
function Get-Process { [CmdletBinding()]param($Name,$Id)
 if($PSBoundParameters.ContainsKey('Name')){return @()}
 return [pscustomobject]@{Id=23456;SessionId=1;StartTime=[DateTime]::UtcNow.AddSeconds(-10)}
}
function schtasks.exe {
 $current=Get-Content -LiteralPath (Join-Path $global:fakeRoot 'current.json') -Raw|ConvertFrom-Json
 if($global:failCandidate -and $current.directory -cne $global:oldDirectory){throw 'injected candidate launch failure'}
 [IO.File]::WriteAllText((Join-Path $global:fakeRoot 'runtime.json'),'{"process_id":23456}')
 [IO.File]::WriteAllText((Join-Path $global:fakeRoot 'worker-status.txt'),'state accepting; jobs running 0')
 Add-Content -LiteralPath (Join-Path $global:fakeRoot 'worker.log') -Value "WORKER STARTED | counts since this process`nSTATE CONNECTED"
 $global:LASTEXITCODE=0
}
function Assert([bool]$Value,[string]$Message){if(!$Value){throw $Message}}
try{
 foreach($case in 'success','launch-failure','tampered-profile'){
  $global:fakeRoot=Join-Path $test $case;$old=Join-Path $global:fakeRoot 'versions\old';$package=Join-Path $test ($case+'-package')
  New-Item -ItemType Directory $old,$package -Force|Out-Null;$global:oldDirectory=$old
  [IO.File]::WriteAllText((Join-Path $old 'OrganizerWorker.exe'),'old fake executable; never launched')
  $oldSha=(Get-FileHash (Join-Path $old 'OrganizerWorker.exe')).Hash.ToLowerInvariant()
  [IO.File]::WriteAllText((Join-Path $global:fakeRoot 'current.json'),([ordered]@{schema=1;directory=$old;sha256=$oldSha}|ConvertTo-Json))
  $cfg=[ordered]@{schema=1;coordinator_url='https://example.invalid';storage_hosts=@('objects.example.invalid');ffmpeg_path='never-run';ffmpeg_sha256=('a'*64);limits=[ordered]@{cpu_threads=2;ram_mb=1024;scratch_mb=2000;max_jobs=2;reserve_ram_mb=0}}
  $configPath=Join-Path $global:fakeRoot 'worker.json';[IO.File]::WriteAllText($configPath,($cfg|ConvertTo-Json -Depth 8));$original=[IO.File]::ReadAllBytes($configPath)
  foreach($name in 'credential.dpapi','pairing-key.dpapi'){[IO.File]::WriteAllText((Join-Path $global:fakeRoot $name),'synthetic protected fixture')}
  [IO.File]::WriteAllText((Join-Path $global:fakeRoot 'worker-status.txt'),'state accepting; jobs running 0')
  [IO.File]::WriteAllText((Join-Path $global:fakeRoot 'worker.log'),'')
  $files=[ordered]@{};foreach($name in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe'){
   [IO.File]::WriteAllText((Join-Path $package $name),('new fake '+$name));$files[$name]=(Get-FileHash (Join-Path $package $name)).Hash.ToLowerInvariant()
  }
  [IO.File]::WriteAllText((Join-Path $package 'package.json'),([ordered]@{schema=1;revision=('1'*40);files=$files}|ConvertTo-Json -Depth 8))
  $profile=Join-Path $test ($case+'-capacity.json');[IO.File]::WriteAllText($profile,'{"cpu_threads":4,"ram_mb":512,"scratch_mb":2000,"max_jobs":4}')
  $profileHash=(Get-FileHash $profile).Hash.ToLowerInvariant();if($case -eq 'tampered-profile'){[IO.File]::AppendAllText($profile,' ')}
  $global:failCandidate=$case -eq 'launch-failure';$failed=$false
  try{& (Join-Path $kit 'install-qualified.ps1') -Root $global:fakeRoot -Package $package -PackageManifestSha256 ((Get-FileHash (Join-Path $package 'package.json')).Hash.ToLowerInvariant()) -CapacityProfile $profile -CapacityProfileSha256 $profileHash}catch{$failed=$true;Write-Output ('EXPECTED_FAILURE='+$_.Exception.Message)}
  $selected=Get-Content (Join-Path $global:fakeRoot 'current.json') -Raw|ConvertFrom-Json
  if($case -eq 'success'){
   Assert (!$failed) 'successful install failed';Assert ($selected.directory -cne $old) 'new version not selected'
   $new=Get-Content $configPath -Raw|ConvertFrom-Json;Assert ($new.limits.cpu_threads -eq 4 -and $new.limits.ram_mb -eq 512) 'qualified limits not published'
   Assert ($new.coordinator_url -ceq $cfg.coordinator_url -and $new.ffmpeg_sha256 -ceq $cfg.ffmpeg_sha256 -and $new.storage_hosts[0] -ceq $cfg.storage_hosts[0]) 'non-limit settings changed'
  }else{Assert $failed 'expected failure not observed';Assert ($selected.directory -ceq $old) 'old version not restored';Assert ([Convert]::ToBase64String([IO.File]::ReadAllBytes($configPath)) -ceq [Convert]::ToBase64String($original)) 'old config not restored byte-for-byte'}
  foreach($name in 'credential.dpapi','pairing-key.dpapi'){Assert ((Get-Content (Join-Path $global:fakeRoot $name) -Raw) -ceq 'synthetic protected fixture') 'protected identity changed'}
  Assert (!(Test-Path (Join-Path $global:fakeRoot 'current.rollback.json'))) 'version recovery marker remains';Assert (!(Test-Path (Join-Path $global:fakeRoot 'capacity.rollback.json'))) 'capacity recovery marker remains'
  Write-Output ('PASS '+$case)
 }
 Write-Output 'QUALIFIED_INSTALL_CONTRACT_EXIT=0'
}finally{Remove-Item -LiteralPath $test -Recurse -Force}
