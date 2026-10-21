param([Parameter(Mandatory=$true)][string]$BinaryDirectory)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-cli-fixture-'+[guid]::NewGuid().ToString('N'))
$ctl=Join-Path $BinaryDirectory 'OrganizerWorkerCtl.exe'
$fixture=Join-Path $BinaryDirectory 'ow_control_fixture.exe'
$p=$null
try {
    & $ctl --help
    if ($LASTEXITCODE -ne 0) { throw 'CLI help failed' }
    $p=Start-Process -FilePath $fixture -ArgumentList ('"'+$root+'"') -PassThru -NoNewWindow
    function Call-Cli([string[]]$Arguments) {
        $raw=& $ctl --json --data-root $root @Arguments
        if ($LASTEXITCODE -ne 0) { throw ('CLI failed: '+($raw -join "`n")) }
        return ($raw -join "`n" | ConvertFrom-Json)
    }
    $until=[DateTime]::UtcNow.AddSeconds(10)
    do {
        Start-Sleep -Milliseconds 100
        $raw=& $ctl --json --data-root $root status
        if ($LASTEXITCODE -eq 0) { break }
    } while ([DateTime]::UtcNow -lt $until)
    $status=$raw | ConvertFrom-Json
    if (-not $status.live -or -not $status.paused -or $status.connected -or $status.accepting) { throw 'Fixture live initial state is incorrect' }
    $status | ConvertTo-Json -Depth 12
    $resume=Call-Cli -Arguments @('resume')
    if ($resume.paused -or $resume.pid -ne $status.pid) { throw 'Resume did not address the same live worker' }
    if ((Call-Cli -Arguments @('resume')).paused) { throw 'Repeated resume toggled pause' }
    $inputJson='{"operator.priority":"low","limits.cpu_threads":4}'
    $changed=$inputJson | & $ctl --json --data-root $root config set --stdin
    if ($LASTEXITCODE -ne 0) { throw 'Atomic config set failed' }
    $changed | Write-Output
    $setting=(Call-Cli -Arguments @('config','get','limits.cpu_threads')).settings[0]
    if ($setting.current -ne 4 -or $setting.effective -ne 2 -or -not $setting.restart_required) { throw 'Cold-setting semantics incorrect' }
    $bad='{"operator.priority":"backup","limits.max_jobs":0}' | & $ctl --json --data-root $root config set --stdin
    if ($LASTEXITCODE -ne 2) { throw 'Invalid config did not return validation exit 2' }
    if ((Call-Cli -Arguments @('config','get','operator.priority')).settings[0].current -ne 'low') { throw 'Invalid update partially persisted' }
    if (-not (Call-Cli -Arguments @('drain')).paused) { throw 'Drain failed' }
    Call-Cli -Arguments @('auth') | ConvertTo-Json -Depth 5
    Call-Cli -Arguments @('config','reset','operator.priority') | ConvertTo-Json -Depth 8
    Call-Cli -Arguments @('exit','drain') | ConvertTo-Json -Depth 8
    if (-not $p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw 'Fixture failed to drain and exit' }
    & $ctl --json --data-root $root status
    if ($LASTEXITCODE -ne 3) { throw 'Stopped worker must be unavailable, not stale live status' }
    Write-Output 'PASS: real native CLI/controller IPC fixture; no coordinator/job/installed-worker acceptance claimed'
} finally {
    if ($p -and -not $p.HasExited) { $p.Kill();$p.WaitForExit() }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
