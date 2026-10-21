param([Parameter(Mandatory=$true)][string]$BinaryDirectory,[Parameter(Mandatory=$true)][string]$BuildDirectory,[string]$Configuration='Release',[Parameter(Mandatory=$true)][string]$CMakeExecutable)
$ErrorActionPreference='Stop'
$root=Join-Path ([IO.Path]::GetTempPath()) ('ow-cli-fixture-'+[guid]::NewGuid().ToString('N'))
$ctl=Join-Path $BinaryDirectory 'OrganizerWorkerCtl.exe'
$fixture=Join-Path $BinaryDirectory 'ow_control_fixture.exe'
$package=Join-Path ([IO.Path]::GetTempPath()) ('ow-cli-package-'+[guid]::NewGuid().ToString('N'))
$p=$null
try {
    & $CMakeExecutable --install $BuildDirectory --config $Configuration --prefix $package
    if ($LASTEXITCODE -ne 0) { throw 'Candidate packaging failed' }
    foreach ($file in 'OrganizerWorker.exe','OrganizerWorkerConsole.exe','OrganizerWorkerDoctor.exe','OrganizerWorkerCtl.exe','OrganizerWorkerTray.exe','start-headless.ps1','cli.md') {
        if (-not (Test-Path -LiteralPath (Join-Path $package $file))) { throw ('Missing packaged file: '+$file) }
    }
    $ctl=Join-Path $package 'OrganizerWorkerCtl.exe'
    & $ctl --help
    if ($LASTEXITCODE -ne 0) { throw 'CLI help failed' }
    $p=Start-Process -FilePath $fixture -ArgumentList ('"'+$root+'"') -PassThru -NoNewWindow
    # Windows PowerShell 5.1 Start-Process may return only a PID-backed Process.
    # Own its handle while alive so ExitCode remains available after exit.
    $ownedFixtureHandle=$p.Handle
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
    if ($LASTEXITCODE -ne 0) { $changed | Write-Output;throw 'Atomic config set failed' }
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
    $exited=$p.WaitForExit(10000)
    $fixtureExitCode=if ($exited) { $p.ExitCode } else { $null }
    if (-not $exited -or $null -eq $fixtureExitCode -or $fixtureExitCode -ne 0) { throw ('Fixture failed to drain and exit: exited={0}; code={1}' -f $exited,$fixtureExitCode) }
    Write-Output ('Fixture drain exit code: '+$fixtureExitCode)
    & $ctl --json --data-root $root status
    if ($LASTEXITCODE -ne 3) { throw 'Stopped worker must be unavailable, not stale live status' }
    Write-Output 'PASS: real native CLI/controller IPC fixture; no coordinator/job/installed-worker acceptance claimed'
} finally {
    if ($p) { if (-not $p.HasExited) { $p.Kill();$p.WaitForExit() };$p.Dispose() }
    if (Test-Path -LiteralPath $root) { Remove-Item -LiteralPath $root -Recurse -Force }
    if (Test-Path -LiteralPath $package) { Remove-Item -LiteralPath $package -Recurse -Force }
}
