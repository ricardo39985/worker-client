param([Parameter(Mandatory=$true)][string]$Root,[Parameter(Mandatory=$true)][string]$BinaryDirectory,
      [Parameter(Mandatory=$true)][string]$Revision,[ValidateSet('configure','pause','resume','finish')][string]$Action)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue';$OutputEncoding=New-Object Text.UTF8Encoding($false)
$ctl=Join-Path $BinaryDirectory 'OrganizerWorkerCtl.exe'
function Check([bool]$Value,[string]$Reason){if(-not $Value){throw $Reason}}
function Wait-Phase([string]$Phase){
    $until=[DateTime]::UtcNow.AddMinutes(10)
    do {
        if(Test-Path (Join-Path $Root 'acceptance-state.json')){
            $s=Get-Content (Join-Path $Root 'acceptance-state.json') -Raw|ConvertFrom-Json
            Check ($s.fixture -and -not $s.real_coordinator) 'Refusing control outside an explicit isolated fixture'
            if($s.phase -eq $Phase){return $s}
        }
        if([DateTime]::UtcNow -ge $until){throw ('Fixture phase timeout: '+$Phase)}
        Start-Sleep -Milliseconds 100
    } while($true)
}
function Call([string[]]$Arguments,[int]$Expected=0){
    $raw=(& $ctl --json --data-root $Root @Arguments) -join "`n";$code=$LASTEXITCODE
    Check ($code -eq $Expected) ('Unexpected CLI exit '+$code+' for '+($Arguments -join ' '))
    $r=$raw|ConvertFrom-Json;Check ($r.schema -eq 1) 'Control schema mismatch';return $r
}
function Patch($Values,[int]$Expected=0){
    $body=ConvertTo-Json -InputObject $Values -Depth 12 -Compress
    $raw=($body|& $ctl --json --data-root $Root config set --stdin) -join "`n";$code=$LASTEXITCODE
    Check ($code -eq $Expected) ('Unexpected atomic patch exit '+$code)
    Check (-not $raw.Contains('SENTINEL_PRIVATE_NOT_A_REAL_SECRET')) 'Rejected input echoed secret-like value'
    return $raw|ConvertFrom-Json
}
if($Action -eq 'configure'){
    $state=Wait-Phase 'ready';$status=Call -Arguments @('status')
    $identity=[Security.Principal.WindowsIdentity]::GetCurrent();$clientSession=(Get-Process -Id $PID).SessionId
    Check ($clientSession -eq 0 -and $state.session -gt 0 -and $clientSession -ne $state.session) 'This gate must actually cross SSH0 to interactive owner session'
    Check ($status.owner_sid -eq $identity.User.Value -and $status.pid -eq $state.pid -and $status.connected) 'Wrong live owner/process/transport state'
    Check ($status.build_revision -eq $Revision) 'Wrong compiled source revision'
    $rows=(Call -Arguments @('config','list')).settings;Check ($rows.Count -eq 14) 'Setting inventory incomplete'
    $original=[ordered]@{};foreach($r in $rows){$original[$r.name]=$r.current}
    $cases=@(@{n='operator.priority';v='backup'},@{n='operator.video_preference';v='disabled'},@{n='operator.image_preference';v='preferred'},@{n='operator.paused';v=$true},@{n='limits.ram_mb';v=3072},@{n='limits.reserve_ram_mb';v=128},@{n='limits.scratch_mb';v=8192},@{n='limits.cpu_threads';v=2},@{n='limits.max_jobs';v=8},@{n='limits.vram_mb';v=256},@{n='storage_hosts';v=@('fixture.invalid','media.fixture.invalid')},@{n='coordinator_url';v='https://FIXTURE.invalid:443/'})
    foreach($case in $cases){$patch=[ordered]@{};$patch[$case.n]=$case.v;$null=Patch $patch;$got=(Call -Arguments @('config','get',$case.n)).settings[0];if($case.n -like 'limits.*' -and $case.v -ne $got.effective){Check $got.restart_required 'Cold setting did not report restart required'}}
    $null=Patch ([ordered]@{ffmpeg_path=$original['ffmpeg_path'];ffmpeg_sha256=$original['ffmpeg_sha256']})
    $invalid=@(@{'limits.cpu_threads'=65},@{'limits.cpu_threads'=1.5},@{'limits.ram_mb'='4096'},@{'operator.priority'='realtime'},@{'operator.paused'='false'},@{storage_hosts=@('*.fixture.invalid')},@{'../credential.dpapi'='SENTINEL_PRIVATE_NOT_A_REAL_SECRET'},@{coordinator_url='https://different.invalid';'operator.priority'='normal'},@{'operator.priority'='normal';'limits.max_jobs'=0})
    foreach($bad in $invalid){$before=(Get-FileHash (Join-Path $Root 'worker.json')).Hash;$null=Patch $bad 2;Check ((Get-FileHash (Join-Path $Root 'worker.json')).Hash -ceq $before) 'Invalid multi-field patch partially persisted'}
    foreach($name in @($original.Keys)){
        if($name -in @('ffmpeg_path','ffmpeg_sha256')){continue}
        if($name -eq 'coordinator_url'){$null=Call -Arguments @('config','reset',$name) -Expected 2;continue}
        $null=Call -Arguments @('config','reset',$name);$r=(Call -Arguments @('config','get',$name)).settings[0]
        Check ((ConvertTo-Json -InputObject $r.current -Compress) -ceq (ConvertTo-Json -InputObject $r.default -Compress)) ('Reset default mismatch '+$name)
    }
    $null=Call -Arguments @('config','reset','ffmpeg_path','ffmpeg_sha256');$null=Patch $original
    $final=Call -Arguments @('status');Check ($final.pid -eq $status.pid -and -not $final.paused) 'Configuration spawned/restarted/paused controller'
    $null=Call -Arguments @('auth');$null=Call -Arguments @('logs');$null=Call -Arguments @('history')
    $proof=[ordered]@{fixture=$true;server_pid=$status.pid;owner_sid=$identity.User.Value;client_session=$clientSession;server_session=$state.session;same_owner_cross_session=$true;all_settings_typed_atomic_reset_readback=$true;source=$Revision}
    [IO.File]::WriteAllText((Join-Path $Root 'client-configuration.json'),($proof|ConvertTo-Json))
    [IO.File]::WriteAllText((Join-Path $Root 'start'),'start')
    Write-Output 'CROSS_SESSION_CONFIG_PASS all14 settings, invalid atomic rejection, defaults/reset, no restart'
} elseif($Action -eq 'pause'){
    $null=Wait-Phase 'child-active';$r=Call -Arguments @('drain');Check $r.paused 'Drain did not pause starts';$r=Call -Arguments @('pause');Check $r.paused 'Repeated pause toggled admission'
    Check ($r.heartbeat.active.Count -ge 2 -and $r.heartbeat.reserved.ram_mb -ge 1024) 'Drain released admitted reservations'
    [IO.File]::WriteAllText((Join-Path $Root 'paused'),'paused');$null=Wait-Phase 'pause-verified'
    Write-Output 'CROSS_SESSION_DRAIN_PASS actual converter survived pause; reserved BEGIN rejected'
} elseif($Action -eq 'resume'){
    $null=Wait-Phase 'pause-verified';$r=Call -Arguments @('resume');Check (-not $r.paused) 'Resume failed';$r=Call -Arguments @('resume');Check (-not $r.paused) 'Repeat resume toggled admission'
    [IO.File]::WriteAllText((Join-Path $Root 'resumed'),'resumed');Write-Output 'CROSS_SESSION_RESUME_PASS explicit idempotent resume'
} else {
    $null=Wait-Phase 'complete';$proof=Get-Content (Join-Path $Root 'acceptance-result.json') -Raw|ConvertFrom-Json
    Check ($proof.actual_typed_children -and $proof.paused_child_survived -and $proof.outbox_acknowledged -and $proof.coordinator_decode_validated -and $proof.jobs.Count -eq 2) 'Real admitted-job/ACK proof incomplete'
    $status=Call -Arguments @('status');Check ($status.pending_results -eq 0 -and $status.heartbeat.active.Count -eq 0) 'Completed attempts/capacity/outbox not drained'
    $p=Get-Process -Id $status.pid;$held=$p.Handle;$null=Call -Arguments @('exit','drain');Check ($p.WaitForExit(15000) -and $p.ExitCode -eq 0) 'Fixture did not exit cleanly';$p.Dispose()
    $null=Call -Arguments @('status') -Expected 3
    Write-Output 'ACTIVE_JOB_COORDINATOR_PASS two real conversions, renewals, retransmission, wrong/matching ACK, natural drain and exit0'
}
