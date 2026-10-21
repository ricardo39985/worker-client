param([Parameter(Mandatory=$true)][string]$BinaryDirectory)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if ($env:GITHUB_ACTIONS -ne 'true') { throw 'Hostile-owner task fixture is restricted to disposable CI, not user PCs' }
$owner=[Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$service='S-1-5-19';$root=Join-Path $env:SystemRoot ('Temp\ow-owner-'+[guid]::NewGuid().ToString('N'))
$tasks=New-Object Collections.Generic.List[string];$p=$null
function Wait-File([string]$Path) {
    $until=[DateTime]::UtcNow.AddSeconds(30)
    while (-not (Test-Path -LiteralPath $Path)) { if ([DateTime]::UtcNow -ge $until) { throw ('Fixture file timeout: '+$Path) };Start-Sleep -Milliseconds 50 }
}
function Protect-Directory([string]$Path,[string]$ServiceRights) {
    $acl=New-Object Security.AccessControl.DirectorySecurity
    $acl.SetAccessRuleProtection($true,$false)
    foreach ($pair in @(@($owner,'FullControl'),@('S-1-5-18','FullControl'),@($service,$ServiceRights))) {
        $sid=New-Object Security.Principal.SecurityIdentifier($pair[0])
        $rule=New-Object Security.AccessControl.FileSystemAccessRule($sid,$pair[1],'ContainerInherit,ObjectInherit','None','Allow')
        $acl.AddAccessRule($rule)
    }
    Set-Acl -LiteralPath $Path -AclObject $acl
}
function Run-Foreign([string[]]$Arguments) {
    $name='OW-Owner-Test-'+[guid]::NewGuid().ToString('N');$tasks.Add($name)
    $quoted=($Arguments|ForEach-Object {'"'+$_+'"'}) -join ' '
    $action=New-ScheduledTaskAction -Execute $exe -Argument $quoted -WorkingDirectory $root
    $principal=New-ScheduledTaskPrincipal -UserId 'NT AUTHORITY\LOCAL SERVICE' -LogonType ServiceAccount -RunLevel Limited
    Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Force | Out-Null
    Start-ScheduledTask -TaskName $name
}
try {
    New-Item -ItemType Directory -Path $root | Out-Null
    Protect-Directory $root 'ReadAndExecute'
    $out=Join-Path $root 'out';$own=Join-Path $root 'owner'
    New-Item -ItemType Directory -Path $out,$own | Out-Null
    Protect-Directory $out 'Modify'
    $exe=Join-Path $root 'owner-fixture.exe';Copy-Item (Join-Path $BinaryDirectory 'ow_owner_fixture.exe') $exe
    $p=Start-Process $exe -ArgumentList ('serve "'+$own+'"') -PassThru -NoNewWindow;$held=$p.Handle
    Wait-File (Join-Path $own 'ready.json');$ready=Get-Content (Join-Path $own 'ready.json') -Raw|ConvertFrom-Json
    & $exe call $own
    if ($LASTEXITCODE -ne 0) { throw 'Correct owner was rejected' }
    $denied=Join-Path $out 'denied.json';Run-Foreign -Arguments @('deny',$ready.pipe,$denied,$owner)
    Wait-File $denied;$proof=Get-Content $denied -Raw|ConvertFrom-Json
    if (-not $proof.denied -or $proof.error -ne 5 -or $proof.caller_sid -ne $service -or $proof.owner_sid -ne $owner) { throw 'Real disallowed owner was not denied by kernel ACL' }
    $proof|ConvertTo-Json -Compress
    $spoof=Join-Path $out 'spoof';New-Item -ItemType Directory -Path $spoof | Out-Null
    $pipe=& $exe name $spoof;if ($LASTEXITCODE -ne 0) { throw 'Pipe-name generation failed' }
    $spoofReady=Join-Path $spoof 'ready';$spoofResult=Join-Path $spoof 'result.json'
    Run-Foreign -Arguments @('spoof',$pipe,$spoofReady,$owner,$spoofResult)
    Wait-File $spoofReady
    & $exe reject $spoof
    if ($LASTEXITCODE -ne 0) { throw 'Wrong-owner server was trusted' }
    Wait-File $spoofResult;$proof=Get-Content $spoofResult -Raw|ConvertFrom-Json
    if (-not $proof.connected -or $proof.server_sid -ne $service -or $proof.client_bytes -ne 0) { throw 'Client sent a request to wrong-owner server' }
    $proof|ConvertTo-Json -Compress
    [IO.File]::WriteAllText((Join-Path $own 'stop'),'stop')
    if (-not $p.WaitForExit(10000) -or $p.ExitCode -ne 0) { throw 'Owned pipe server did not exit cleanly' }
    Write-Output 'OWNER_CONTEXT_PASS real foreign SID denied; forged server received zero request bytes'
} finally {
    foreach ($name in $tasks) { Stop-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue;Unregister-ScheduledTask -TaskName $name -Confirm:$false -ErrorAction SilentlyContinue }
    foreach ($name in $tasks) { if (Get-ScheduledTask -TaskName $name -ErrorAction SilentlyContinue) { throw ('Fixture task retained: '+$name) } }
    if ($p) { if (-not $p.HasExited) { $p.Kill();$p.WaitForExit() };$p.Dispose() }
    if (Test-Path $root) { Remove-Item -LiteralPath $root -Recurse -Force }
}
